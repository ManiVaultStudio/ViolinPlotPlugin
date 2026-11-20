#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace violin {

struct Options
{
    bool    log1p = false;
    bool    includeZeros = true;
    double  bandwidthScale = 1.0;
};

struct DensityPoint
{
    double value   = 0.0;
    double density = 0.0;
};

struct Distribution
{
    std::size_t                 total = 0;
    std::size_t                 finite = 0;
    std::size_t                 detected = 0;
    std::size_t                 count = 0;
    std::size_t                 invalid = 0;
    std::size_t                 negativeLogValues = 0;
    double                      minimum = 0;
    double                      maximum = 0;
    double                      mean = 0;
    double                      q1 = 0;
    double                      median = 0;
    double                      q3 = 0;
    double                      bandwidth = 0;
    double                      lower = 0;
    double                      upper = 1;
    double                      peakDensity = 0;
    bool                        constant = false;
    std::vector<DensityPoint>   density;
    std::vector<double>         dots;
};

inline double quantile(const std::vector<double>& sortedValues, double probability)
{
    if (sortedValues.empty())
        return 0;

    const double position      = probability * static_cast<double>(sortedValues.size() - 1);
    const auto lowerValueIndex = static_cast<std::size_t>(position);
    const auto upperValueIndex = std::min(lowerValueIndex + 1, sortedValues.size() - 1);

    return sortedValues[lowerValueIndex] + (position - lowerValueIndex) * (sortedValues[upperValueIndex] - sortedValues[lowerValueIndex]);
}

// All observations contribute. Only the optional display dots are sampled.
// A linearly binned Gaussian KDE bounds the cost independently of cell count.
inline Distribution summarize(const std::vector<float>& rawExpressionValues, const Options& options)
{
    Distribution distribution;

    distribution.total = rawExpressionValues.size();

    std::vector<double> expressionValues;

    expressionValues.reserve(rawExpressionValues.size());

    bool allValuesNonnegative = true;
    constexpr std::size_t maximumSampledDots = 180;

    distribution.dots.reserve(std::min(maximumSampledDots, rawExpressionValues.size()));

    std::uint64_t randomState = 0x8a5cd789635d2dffULL;

    const auto nextRandomValue = [&randomState]() {
        randomState ^= randomState >> 12;
        randomState ^= randomState << 25;
        randomState ^= randomState >> 27;
        return randomState * 2685821657736338717ULL;
    };
    for (const float rawValue : rawExpressionValues)
    {
        if (!std::isfinite(rawValue)) {
            ++distribution.invalid;
            continue;
        }

        ++distribution.finite;

        if (rawValue > 0)
            ++distribution.detected;

        if (!options.includeZeros && rawValue == 0)
            continue;

        if (options.log1p && rawValue < 0) {
            ++distribution.negativeLogValues;
            continue;
        }

        const double value = options.log1p ? std::log1p(double(rawValue)) : double(rawValue);

        allValuesNonnegative = allValuesNonnegative && value >= 0;
        expressionValues.push_back(value);

        // Reservoir sampling keeps real observations, without favoring rare tails.
        if (distribution.dots.size() < maximumSampledDots)
            distribution.dots.push_back(value);
        else {
            const auto index = nextRandomValue() % expressionValues.size();

            if (index < maximumSampledDots)
                distribution.dots[static_cast<std::size_t>(index)] = value;
        }
    }

    distribution.count = expressionValues.size();

    if (expressionValues.empty())
        return distribution;

    std::sort(expressionValues.begin(), expressionValues.end());
    distribution.minimum = expressionValues.front();
    distribution.maximum = expressionValues.back();
    distribution.q1      = quantile(expressionValues, 0.25);
    distribution.median  = quantile(expressionValues, 0.5);
    distribution.q3      = quantile(expressionValues, 0.75);

    // Welford's algorithm avoids cancellation for nearly constant values.
    double sumSquaredDeviations = 0;
    std::size_t processedValueCount = 0;

    for (const double value : expressionValues)
    {
        const double delta = value - distribution.mean;

        distribution.mean += delta / double(++processedValueCount);
        sumSquaredDeviations += delta * (value - distribution.mean);
    }

    distribution.constant = distribution.maximum == distribution.minimum;
    if (distribution.constant) {
        const double padding = std::max(0.05, std::abs(distribution.minimum) * 0.05);

        distribution.lower = allValuesNonnegative ? std::max(0.0, distribution.minimum - padding) : distribution.minimum - padding;
        distribution.upper = distribution.maximum + padding;
        return distribution;
    }

    const double standardDeviation      = std::sqrt(std::max(0.0, sumSquaredDeviations / double(expressionValues.size() - 1)));
    const double interquartileRangeScale = (distribution.q3 - distribution.q1) / 1.34;
    const double estimatedSpread        = interquartileRangeScale > 0 ? std::min(standardDeviation, interquartileRangeScale) : standardDeviation;
    const double bandwidthScale         = std::isfinite(options.bandwidthScale) ? std::clamp(options.bandwidthScale, 0.25, 4.0) : 1.0;

    distribution.bandwidth = std::max((distribution.maximum - distribution.minimum) * 1e-6, 0.9 * estimatedSpread * std::pow(double(expressionValues.size()), -0.2) * bandwidthScale);
    distribution.lower     = distribution.minimum - 3 * distribution.bandwidth;

    if (allValuesNonnegative)
        distribution.lower = std::max(0.0, distribution.lower);

    distribution.upper = distribution.maximum + 3 * distribution.bandwidth;

    constexpr std::size_t histogramBinCount  = 256;
    constexpr std::size_t densitySampleCount = 160;
    std::array<double, histogramBinCount> histogram{};
    const double histogramBinWidth = (distribution.upper - distribution.lower) / double(histogramBinCount - 1);

    // Interpolate each observation between its nearest bins to reduce aliasing.
    for (const double value : expressionValues)
    {
        const double position = std::clamp((value - distribution.lower) / histogramBinWidth, 0.0, double(histogramBinCount - 1));
        const auto bin        = static_cast<std::size_t>(position);
        const double fraction = position - double(bin);

        histogram[bin] += 1 - fraction;
        histogram[std::min(bin + 1, histogramBinCount - 1)] += fraction;
    }

    const double densityNormalizationFactor = 1 / (double(expressionValues.size()) * distribution.bandwidth * std::sqrt(2 * 3.14159265358979323846));

    distribution.density.reserve(densitySampleCount);

    for (std::size_t densitySampleIndex = 0; densitySampleIndex < densitySampleCount; ++densitySampleIndex)
    {
        const double sampleValue = distribution.lower + (distribution.upper - distribution.lower) * double(densitySampleIndex) / double(densitySampleCount - 1);
        double density = 0;

        for (std::size_t bin = 0; bin < histogramBinCount; ++bin)
        {
            if (histogram[bin] == 0)
                continue;

            const double value                = distribution.lower + double(bin) * histogramBinWidth;
            const double standardizedDistance = (sampleValue - value) / distribution.bandwidth;

            density += histogram[bin] * std::exp(-0.5 * standardizedDistance * standardizedDistance);

            // Reflection at zero keeps expression density on its valid domain.
            if (allValuesNonnegative) {
                const double reflectedStandardizedDistance = (sampleValue + value) / distribution.bandwidth;

                density += histogram[bin] * std::exp(-0.5 * reflectedStandardizedDistance * reflectedStandardizedDistance);
            }
        }

        density *= densityNormalizationFactor;
        distribution.peakDensity = std::max(distribution.peakDensity, density);
        distribution.density.push_back({sampleValue, density});
    }

    return distribution;
}

} // namespace violin
