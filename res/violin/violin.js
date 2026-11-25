/* Full, symmetric KDE violins: d3.scaleBand + d3.area, following the pattern
 * illustrated at https://d3-graph-gallery.com/graph/violin_basicDens.html.
 * Densities and exact summaries are computed in C++ from all plotted cells.
 * This renderer uses equal maximum widths and trims each KDE to observed values.
 */
(() => {
    "use strict";
    const d3 = window.d3;
    if (!d3) throw new Error("The bundled D3 library could not be loaded.");
    const svg = d3.select("#violin-chart");
    const container = document.getElementById("chart-scroll");
    const empty = document.getElementById("empty-message");
    const tooltip = document.getElementById("tooltip");
    const defaults = { sharedAxis: false, showDots: true, log1p: false, includeZeros: true };
    let data = { groups: [], panels: [] }, options = { ...defaults };
    let selected = new Set(), message = empty.textContent, frame = 0;
    let onGroupActivated = () => {}, onError = () => {};
    const panelHeight = 312, maxTotalDots = 12000;
    const fmt = d3.format(".4~g");
    const numeric = value => Number.isFinite(Number(value)) ? Number(value) : 0;
    const short = (text, length) => text.length > length ? text.slice(0, length - 1) + "…" : text;

    // Embedded in exports so the SVG remains self-contained outside Qt.
    const exportStyle = `
      .plot {font-family:Arial,Helvetica,sans-serif;font-size:12px;background:#fff}
      .plot text {fill:#223047}
      .plot .panel-title {font-size:16px;font-weight:600}
      .plot .subtitle,.plot .axis-label,.plot .sample-count {fill:#627188}
      .plot .subtitle,.plot .sample-count {font-size:11px}
      .plot .grid line {stroke:#e4e9f0}.plot .grid .domain {display:none}
      .plot .axis path,.plot .axis line {stroke:#9aa7b6}.plot .axis text {fill:#526176}
      .plot .violin {fill-opacity:.72;stroke-width:1.1}
      .plot .cell-dot {fill:#223047;fill-opacity:.28}
      .plot .range-line {stroke:#223047;stroke-width:1}
      .plot .iqr-line {stroke:#223047;stroke-width:4;stroke-linecap:round}
      .plot .median {fill:white;stroke:#223047;stroke-width:1}
      .plot .constant-line {stroke-width:2}
      .plot .group-highlight {fill:#3b82f6;fill-opacity:0}
      .plot .selected .group-highlight {fill-opacity:.09}
      .plot .selected .violin {stroke-width:2.2}
    `;

    function normalizeDistribution(input = {}) {
        const d = { ...input };
        for (const key of ["count", "finite", "total", "detected", "invalid", "negativeLogValues",
            "minimum", "maximum", "mean", "q1", "median", "q3", "bandwidth"])
            d[key] = numeric(input[key]);
        d.count = Math.max(0, d.count);
        d.density = Array.isArray(input.density) ? input.density
            .filter(p => Array.isArray(p) && p.length === 2 && p.every(Number.isFinite) && p[1] >= 0)
            .map(p => p.slice()).sort((a, b) => a[0] - b[0]) : [];
        d.dots = Array.isArray(input.dots) ? input.dots.filter(Number.isFinite) : [];
        d.constant = Boolean(input.constant) || d.minimum === d.maximum;
        return d;
    }

    function normalizeData(input) {
        const parsed = typeof input === "string" ? JSON.parse(input) : input;
        if (!parsed || !Array.isArray(parsed.groups) || !Array.isArray(parsed.panels))
            throw new Error("Invalid violin chart payload.");
        return {
            groups: parsed.groups.map((g, index) => ({
                name: String(g.name ?? `Group ${index + 1}`),
                color: d3.color(g.color) ? String(g.color) : "#7299cf",
                clusterIndex: Number.isInteger(g.clusterIndex) ? g.clusterIndex : -1
            })),
            panels: parsed.panels.map(p => ({
                name: String(p.name ?? "Feature"),
                distributions: parsed.groups.map((_, i) => normalizeDistribution(p.distributions?.[i]))
            }))
        };
    }

    function domainFor(distributions) {
        const valid = distributions.filter(d => d.count > 0);
        if (!valid.length) return [0, 1];
        let lower = Math.min(0, d3.min(valid, d => d.minimum));
        let upper = d3.max(valid, d => d.maximum);
        if (lower === upper) upper = lower + 1;
        const padding = (upper - lower) * 0.06;
        if (lower < 0) lower -= padding;
        return [lower, upper + padding];
    }

    function trimmedDensity(d) {
        if (!d.count || d.constant || !d.density.length) return [];
        const points = d.density;
        const at = value => {
            const i = d3.bisector(p => p[0]).left(points, value);
            if (i === 0) return points[0][1];
            if (i >= points.length) return points[points.length - 1][1];
            const a = points[i - 1], b = points[i];
            const t = b[0] === a[0] ? 0 : (value - a[0]) / (b[0] - a[0]);
            return a[1] + t * (b[1] - a[1]);
        };
        return [[d.minimum, at(d.minimum)],
            ...points.filter(p => p[0] > d.minimum && p[0] < d.maximum),
            [d.maximum, at(d.maximum)]];
    }

    function sampleDots(values, limit) {
        if (values.length <= limit) return values;
        // A plot-wide budget may reduce the C++ reservoir further. Shuffle before
        // truncating so small groups stored in input order are not biased.
        const copy = values.slice();
        let state = 0x6d2b79f5;
        for (let i = copy.length - 1; i > 0; --i) {
            state ^= state << 13; state ^= state >>> 17; state ^= state << 5;
            const j = (state >>> 0) % (i + 1);
            [copy[i], copy[j]] = [copy[j], copy[i]];
        }
        return copy.slice(0, limit);
    }

    function scheduleRender() {
        if (!frame) frame = requestAnimationFrame(() => {
            frame = 0;
            try { render(); } catch (error) { onError(String(error.message || error)); }
        });
    }

    function applySelection() {
        svg.selectAll(".group").classed("selected", d => selected.has(d.group.clusterIndex))
            .attr("aria-pressed", d => String(selected.has(d.group.clusterIndex)));
    }

    function activate(event, group) {
        if (group.clusterIndex < 0) return;
        const toggle = Boolean(event.ctrlKey || event.metaKey);
        if (!toggle) selected = new Set([group.clusterIndex]);
        else if (selected.has(group.clusterIndex)) selected.delete(group.clusterIndex);
        else selected.add(group.clusterIndex);
        applySelection();
        onGroupActivated(group.clusterIndex, toggle);
    }

    function showTooltip(event, panel, group, d) {
        const detected = d.finite > 0 ? (100 * d.detected / d.finite).toFixed(1) : "0.0";
        tooltip.textContent = `${panel.name} · ${group.name}\n` +
            `Plotted: ${d.count.toLocaleString()} / ${d.finite.toLocaleString()} finite cells\n` +
            `Above zero: ${detected}% of finite values\n` +
            (d.count ? `Median: ${fmt(d.median)}   Mean: ${fmt(d.mean)}\nIQR: ${fmt(d.q1)}–${fmt(d.q3)}\n` : "No values after filtering\n") +
            `Nonfinite skipped: ${d.invalid}; negatives skipped for log1p: ${d.negativeLogValues}\n` +
            (group.clusterIndex >= 0 ? "Click: select group · Ctrl-click: toggle" : "All loaded cells / spots");
        tooltip.hidden = false;
        const x = event.clientX ?? 0, y = event.clientY ?? 0;
        tooltip.style.left = `${Math.max(4, Math.min(x + 14, innerWidth - tooltip.offsetWidth - 8))}px`;
        tooltip.style.top = `${Math.max(4, Math.min(y + 14, innerHeight - tooltip.offsetHeight - 8))}px`;
    }

    function render() {
        tooltip.hidden = true;
        const hasData = data.groups.length > 0 && data.panels.length > 0;
        empty.hidden = hasData;
        empty.textContent = message;
        svg.attr("hidden", hasData ? null : true);
        svg.selectAll("*").remove();
        if (!hasData) return;
        const width = Math.max(container.clientWidth, 640, 106 + data.groups.length * 92);
        const height = data.panels.length * panelHeight + 12;
        const left = 76, right = 30, plotWidth = width - left - right, plotHeight = 188;
        const dotLimit = Math.min(180, Math.floor(maxTotalDots / (data.panels.length * data.groups.length)));
        svg.attr("width", width).attr("height", height).attr("viewBox", `0 0 ${width} ${height}`);
        svg.append("title").text("Gene expression distributions by cell type or spatial region");
        svg.append("desc").text("Full symmetric KDE violins, equal maximum widths, trimmed to observed values. Medians and interquartile ranges are shown. Optional cell dots are sampled.");
        const defs = svg.append("defs");
        const sharedDomain = domainFor(data.panels.flatMap(p => p.distributions));

        data.panels.forEach((panel, pi) => {
            const root = svg.append("g").attr("class", "gene-panel").attr("transform", `translate(0,${16 + pi * panelHeight})`);
            root.append("text").attr("class", "panel-title").attr("x", left).attr("y", 18)
                .text(short(panel.name, 80)).append("title").text(panel.name);
            const plot = root.append("g").attr("transform", `translate(${left},36)`);
            const domain = options.sharedAxis ? sharedDomain : domainFor(panel.distributions);
            const y = d3.scaleLinear().domain(domain).range([plotHeight, 0]);
            const x = d3.scaleBand().domain(d3.range(data.groups.length)).range([0, plotWidth]).padding(0.24);
            const halfWidth = Math.min(34, x.bandwidth() / 2);
            plot.attr("data-y-min", domain[0]).attr("data-y-max", domain[1]);
            plot.append("g").attr("class", "grid").call(d3.axisLeft(y).ticks(4).tickSize(-plotWidth).tickFormat(""));
            plot.append("g").attr("class", "axis y-axis").call(d3.axisLeft(y).ticks(4).tickFormat(fmt));
            const axis = plot.append("g").attr("class", "axis x-axis").attr("transform", `translate(0,${plotHeight})`)
                .call(d3.axisBottom(x).tickSizeOuter(0).tickFormat(i => short(data.groups[i].name, 17)));
            axis.selectAll(".tick text").attr("text-anchor", "end").attr("transform", "translate(-3,5) rotate(-28)")
                .append("title").text(i => data.groups[i].name);
            root.append("text").attr("class", "axis-label").attr("text-anchor", "middle")
                .attr("transform", `translate(17,${36 + plotHeight / 2}) rotate(-90)`)
                .text(options.log1p ? "ln(1 + expression)" : "Expression");
            const clipId = `violin-panel-${pi}`;
            defs.append("clipPath").attr("id", clipId).append("rect")
                .attr("x", 0).attr("y", -3).attr("width", plotWidth).attr("height", plotHeight + 6);

            const groups = plot.selectAll(".group").data(data.groups.map((group, gi) => ({ group, gi, d: panel.distributions[gi] })))
                .join("g").attr("class", value => "group" + (value.group.clusterIndex >= 0 ? " selectable" : ""))
                .attr("data-cluster-index", value => value.group.clusterIndex)
                .attr("tabindex", value => value.group.clusterIndex >= 0 ? 0 : null)
                .attr("role", value => value.group.clusterIndex >= 0 ? "button" : "img")
                .attr("aria-label", value => `${panel.name}, ${value.group.name}, ${value.d.count} plotted cells`)
                .on("click", (event, value) => activate(event, value.group))
                .on("keydown", (event, value) => {
                    if (event.key === "Enter" || event.key === " ") { event.preventDefault(); activate(event, value.group); }
                })
                .on("mousemove", (event, value) => showTooltip(event, panel, value.group, value.d))
                .on("mouseleave", () => { tooltip.hidden = true; });

            groups.each(function ({ group, gi, d }) {
                const g = d3.select(this), center = x(gi) + x.bandwidth() / 2;
                const slot = x.step();
                g.append("rect").attr("class", "group-highlight").attr("x", center - slot / 2)
                    .attr("y", -3).attr("width", slot).attr("height", plotHeight + 75);
                const marks = g.append("g").attr("clip-path", `url(#${clipId})`);
                if (!d.count) {
                    g.append("text").attr("class", "subtitle").attr("x", center).attr("y", plotHeight / 2)
                        .attr("text-anchor", "middle").text("No values");
                } else {
                    if (d.constant) {
                        marks.append("line").attr("class", "constant-line").attr("stroke", group.color)
                            .attr("x1", center - halfWidth).attr("x2", center + halfWidth)
                            .attr("y1", y(d.median)).attr("y2", y(d.median));
                    } else {
                        const density = trimmedDensity(d), peak = d3.max(density, point => point[1]) || 1;
                        const w = d3.scaleLinear().domain([0, peak]).range([0, halfWidth]);
                        const area = d3.area().x0(point => center - w(point[1])).x1(point => center + w(point[1]))
                            .y(point => y(point[0])).curve(d3.curveMonotoneY);
                        marks.append("path").datum(density).attr("class", "violin")
                            .attr("fill", group.color).attr("stroke", d3.color(group.color).darker(0.6).formatHex()).attr("d", area);
                    }
                    if (options.showDots && dotLimit > 0) {
                        const dots = sampleDots(d.dots, dotLimit);
                        marks.selectAll(".cell-dot").data(dots).join("circle").attr("class", "cell-dot")
                            .attr("r", 1.5).attr("cy", value => y(value)).attr("cx", (_, i) => {
                                const hash = (Math.imul(i + 1, 2654435761) ^ Math.imul(gi + 1, 2246822519)) >>> 0;
                                return center + ((hash % 10000) / 9999 - 0.5) * halfWidth * 1.6;
                            });
                    }
                    marks.append("line").attr("class", "range-line").attr("x1", center).attr("x2", center)
                        .attr("y1", y(d.minimum)).attr("y2", y(d.maximum));
                    marks.append("line").attr("class", "iqr-line").attr("x1", center).attr("x2", center)
                        .attr("y1", y(d.q1)).attr("y2", y(d.q3));
                    marks.append("circle").attr("class", "median").attr("cx", center).attr("cy", y(d.median)).attr("r", 3);
                }
                g.append("text").attr("class", "sample-count").attr("x", center).attr("y", plotHeight + 68)
                    .attr("text-anchor", "middle").text(`n=${d.count.toLocaleString()}`);
                g.append("rect").attr("class", "group-hit").attr("x", center - slot / 2).attr("y", -3)
                    .attr("width", slot).attr("height", plotHeight + 75);
            });
        });
        applySelection();
    }

    function exportSvg() {
        if (frame) { cancelAnimationFrame(frame); frame = 0; }
        render();
        if (!data.groups.length || !data.panels.length) throw new Error("There is no plot to export.");
        const clone = svg.node().cloneNode(true);
        clone.removeAttribute("hidden");
        clone.querySelectorAll(".group-hit").forEach(node => node.remove());
        clone.querySelectorAll("[tabindex]").forEach(node => node.removeAttribute("tabindex"));
        const background = document.createElementNS("http://www.w3.org/2000/svg", "rect");
        background.setAttribute("width", "100%"); background.setAttribute("height", "100%"); background.setAttribute("fill", "white");
        clone.insertBefore(background, clone.firstChild);
        const style = document.createElementNS("http://www.w3.org/2000/svg", "style");
        style.textContent = exportStyle;
        clone.insertBefore(style, clone.firstChild);
        return new XMLSerializer().serializeToString(clone);
    }

    async function exportPlot(format) {
        const source = exportSvg();
        if (format === "svg") return source;
        if (format !== "png") throw new Error("Unsupported export format.");
        const width = Number(svg.attr("width")), height = Number(svg.attr("height")), scale = 2;
        if (width * height * scale * scale > 40000000)
            throw new Error("This PNG would exceed 40 million pixels. Export SVG or select fewer genes.");
        const url = URL.createObjectURL(new Blob([source], { type: "image/svg+xml;charset=utf-8" }));
        try {
            const image = await new Promise((resolve, reject) => {
                const img = new Image(); img.onload = () => resolve(img);
                img.onerror = () => reject(new Error("The SVG could not be rasterized.")); img.src = url;
            });
            const canvas = document.createElement("canvas");
            canvas.width = width * scale; canvas.height = height * scale;
            const context = canvas.getContext("2d");
            if (!context) throw new Error("PNG export is unavailable.");
            context.fillStyle = "white"; context.fillRect(0, 0, canvas.width, canvas.height);
            context.drawImage(image, 0, 0, canvas.width, canvas.height);
            return canvas.toDataURL("image/png");
        } finally { URL.revokeObjectURL(url); }
    }

    window.ViolinChart = {
        setData(input) { data = normalizeData(input); scheduleRender(); },
        setOptions(input) {
            const parsed = typeof input === "string" ? JSON.parse(input) : input;
            options = { ...defaults, ...parsed }; scheduleRender();
        },
        setSelection(input) { selected = new Set(Array.from(input || []).map(Number)); applySelection(); },
        setMessage(input) { message = String(input); if (!data.panels.length) empty.textContent = message; },
        onGroupActivated(callback) { onGroupActivated = callback; },
        onError(callback) { onError = callback; },
        exportPlot,
        // Pure helpers also used in numerical/renderer contract tests.
        domainFor, trimmedDensity, sampleDots
    };
    new ResizeObserver(scheduleRender).observe(container);
    scheduleRender();
})();
