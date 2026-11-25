#include "ViolinPlotPlugin.h"

#include <ClusterData/ClusterData.h>
#include <PointData/PointData.h>
#include <DatasetsMimeData.h>
#include <actions/PluginTriggerAction.h>
#include <widgets/DropWidget.h>

#include <QScopedValueRollback>
#include <QSplitter>
#include <QVBoxLayout>

#include <algorithm>
#include <exception>
#include <limits>
#include <numeric>
#include <stdexcept>
#include <unordered_map>

using namespace mv;
using namespace mv::gui;
using namespace mv::plugin;

ViolinPlotPlugin::ViolinPlotPlugin(const PluginFactory* pluginFactory) :
    ViewPlugin(pluginFactory),
    _settingsAction(this, "Settings")
{
    _updateDebounceTimer.setSingleShot(true);
    _updateDebounceTimer.setInterval(75);
    _geneCalculationTimer.setInterval(0);
    connect(&_updateDebounceTimer, &QTimer::timeout, this, &ViolinPlotPlugin::beginPlotUpdate);
    connect(&_geneCalculationTimer, &QTimer::timeout, this, &ViolinPlotPlugin::calculateNextGeneDistribution);
}

void ViolinPlotPlugin::init()
{
    auto* mainLayout = new QVBoxLayout;

    mainLayout->setContentsMargins(8, 8, 8, 8);
    mainLayout->setSpacing(6);
    getWidget().setLayout(mainLayout);
    mainLayout->addWidget(_settingsAction.createWidget(&getWidget()));

    auto* genePlotSplitter = new QSplitter;

    genePlotSplitter->addWidget(_settingsAction.createSelectionWidget(genePlotSplitter));

    auto* plotAreaWidget = new QWidget(genePlotSplitter);
    auto* plotAreaLayout = new QVBoxLayout(plotAreaWidget);

    plotAreaLayout->setContentsMargins(0, 0, 0, 0);
    plotAreaLayout->addWidget(_settingsAction.createTitleWidget(plotAreaWidget));
    _violinPlotWidget = new ViolinPlotWidget;
    plotAreaLayout->addWidget(_violinPlotWidget, 1);
    genePlotSplitter->addWidget(plotAreaWidget);
    genePlotSplitter->setStretchFactor(1, 1);
    genePlotSplitter->setSizes({240, 800});
    mainLayout->addWidget(genePlotSplitter, 1);
    mainLayout->addWidget(_settingsAction.createStatusWidget(&getWidget()));

    _settingsAction.initialize(this);

    _dropWidget = new DropWidget(_violinPlotWidget);
    _dropWidget->setDropIndicatorWidget(new DropWidget::DropIndicatorWidget(&getWidget(), "Violin plots", "Drag expression Points data here. Add a Clusters dataset to group cells or spots."));
    _dropWidget->initialize([this](const QMimeData* mimeData) -> DropWidget::DropRegions {
        DropWidget::DropRegions dropRegions;
        const auto* datasetsMimeData = dynamic_cast<const DatasetsMimeData*>(mimeData);

        if (datasetsMimeData == nullptr || datasetsMimeData->getDatasets().isEmpty())
            return dropRegions;

        const auto datasets = datasetsMimeData->getDatasets();
        bool hasPointDataset   = false;
        bool hasClusterDataset = false;

        for (const auto& dataset : datasets)
        {
            if (!dataset.isValid())
                return dropRegions;

            hasPointDataset   |= dataset->getDataType() == PointType;
            hasClusterDataset |= dataset->getDataType() == ClusterType;
        }

        const auto isSingleDataset = datasets.size() == 1 && (hasPointDataset || hasClusterDataset);
        const auto isDatasetPair   = datasets.size() == 2 && hasPointDataset && hasClusterDataset;

        if (isSingleDataset || isDatasetPair) {
            if (!hasPointDataset && _pointDataset.isValid() && !isCompatibleClusterDataset(Dataset<Clusters>(datasets.first()))) {
                dropRegions << new DropWidget::DropRegion(this, "Incompatible groups", "Load the corresponding expression Points dataset first.", "exclamation-circle", false);
            } else {
                dropRegions << new DropWidget::DropRegion(this, "Violin plots", "Plot gene expression by cells or clusters", "chart-bar", true, [this, datasets] {
                    loadData(datasets);
                });
            }
        }

        return dropRegions;
    });

    connect(_violinPlotWidget, &ViolinPlotWidget::groupActivated, this, &ViolinPlotPlugin::activateClusterGroup);
    connect(&_pointDataset, &Dataset<Points>::dataChanged, this, &ViolinPlotPlugin::schedulePlotUpdate);
    connect(&_pointDataset, &Dataset<Points>::removed, this, [this] {
        setPointDataset({});
    });
    connect(&_pointDataset, &Dataset<Points>::guiNameChanged, this, &ViolinPlotPlugin::schedulePlotUpdate);
    connect(&_clusterDataset, &Dataset<Clusters>::dataChanged, this, &ViolinPlotPlugin::updateCellTypeSelection);
    connect(&_clusterDataset, &Dataset<Clusters>::removed, this, [this] {
        setClusterDataset({});
    });
    connect(&_clusterDataset, &Dataset<Clusters>::guiNameChanged, this, &ViolinPlotPlugin::schedulePlotUpdate);
    connect(&_clusterDataset, &Dataset<Clusters>::dataSelectionChanged, this, &ViolinPlotPlugin::synchronizeClusterSelection);

    if (_hasDeferredPluginState) {
        _hasDeferredPluginState = false;
        fromVariantMap(_deferredPluginState);
        _deferredPluginState.clear();
    }

    if (!_deferredDatasets.isEmpty()) {
        const auto datasets = _deferredDatasets;

        _deferredDatasets.clear();
        loadData(datasets);
    }
}

bool ViolinPlotPlugin::isCompatibleClusterDataset(const Dataset<Clusters>& clusterDataset) const
{
    return violin::DatasetsAction::isCompatibleGrouping(_pointDataset, clusterDataset);
}

void ViolinPlotPlugin::loadData(const Datasets& datasets)
{
    if (_violinPlotWidget == nullptr) {
        _deferredDatasets = datasets;
        return;
    }

    Dataset<Points> pointDataset;
    Dataset<Clusters> clusterDataset;

    for (const auto& dataset : datasets)
    {
        if (!dataset.isValid())
            continue;

        if (dataset->getDataType() == PointType && !pointDataset.isValid())
            pointDataset = Dataset<Points>(dataset);

        if (dataset->getDataType() == ClusterType && !clusterDataset.isValid())
            clusterDataset = Dataset<Clusters>(dataset);
    }

    if (!pointDataset.isValid() && !_pointDataset.isValid() && clusterDataset.isValid()) {
        const auto parentDataset = clusterDataset->getParent();

        if (parentDataset.isValid() && parentDataset->getDataType() == PointType)
            pointDataset = Dataset<Points>(parentDataset);
    }

    if (pointDataset.isValid())
        setPointDataset(pointDataset);

    if (clusterDataset.isValid())
        setClusterDataset(clusterDataset);
}

void ViolinPlotPlugin::setPointDataset(const Dataset<Points>& pointDataset)
{
    _geneCalculationTimer.stop();
    _pointDataset = pointDataset;

    {
        // Keep the pickers' own signals enabled so all popup widgets stay synchronized.
        const QScopedValueRollback<bool> pickerSynchronizationGuard(_isSynchronizingDatasetPickers, true);
        auto& datasetsAction = _settingsAction.getDatasetsAction();

        datasetsAction.getPointDatasetPickerAction().setCurrentDataset(_pointDataset);
        datasetsAction.updateClusterDatasetFilter(_pointDataset);
        datasetsAction.getClusterDatasetPickerAction().setCurrentDataset(_clusterDataset);
    }

    if (_clusterDataset.isValid() && !isCompatibleClusterDataset(_clusterDataset))
        setClusterDataset({});

    updateGeneSelection();
}

void ViolinPlotPlugin::setClusterDataset(const Dataset<Clusters>& clusterDataset)
{
    if (clusterDataset.isValid() && !_pointDataset.isValid()) {
        const auto parentDataset = clusterDataset->getParent();

        if (parentDataset.isValid() && parentDataset->getDataType() == PointType)
            setPointDataset(Dataset<Points>(parentDataset));
    }

    if (clusterDataset.isValid() && !isCompatibleClusterDataset(clusterDataset)) {
        const QScopedValueRollback<bool> pickerSynchronizationGuard(_isSynchronizingDatasetPickers, true);

        _settingsAction.getDatasetsAction().getClusterDatasetPickerAction().setCurrentDataset(_clusterDataset);
        _groupingWarningMessage = "Groups were not loaded: the Clusters dataset must belong to the same source Points data.";
        _settingsAction.getStatusMessageAction().setString(_groupingWarningMessage);
        return;
    }

    _clusterDataset = clusterDataset;
    _groupingWarningMessage.clear();

    {
        const QScopedValueRollback<bool> pickerSynchronizationGuard(_isSynchronizingDatasetPickers, true);

        _settingsAction.getDatasetsAction().getClusterDatasetPickerAction().setCurrentDataset(_clusterDataset);
    }

    synchronizeClusterSelection();
    updateCellTypeSelection();
}

void ViolinPlotPlugin::updateGeneSelection()
{
    _settingsAction.getGeneSelectionAction().setPointsDataset(_pointDataset);
    schedulePlotUpdate();
}

void ViolinPlotPlugin::updateCellTypeSelection()
{
    std::vector<violin::CellTypeEntry> cellTypes;
    QString groupingDatasetId;

    if (_clusterDataset.isValid()) {
        groupingDatasetId = _clusterDataset->getId();
        const auto& clusters = _clusterDataset->getClusters();

        cellTypes.reserve(clusters.size());

        for (int clusterIndex = 0; clusterIndex < clusters.size(); ++clusterIndex)
        {
            const auto& cluster = clusters[clusterIndex];

            cellTypes.push_back({cluster.getId(), clusterIndex, cluster.getName(), cluster.getColor(), true});
        }
    }

    _settingsAction.getCellTypeSelectionAction().setCellTypes(groupingDatasetId, cellTypes);
    schedulePlotUpdate();
}

void ViolinPlotPlugin::schedulePlotUpdate()
{
    _geneCalculationTimer.stop();
    _updateDebounceTimer.start();
    _settingsAction.setExportEnabled(false);
}

void ViolinPlotPlugin::beginPlotUpdate()
{
    _geneCalculationTimer.stop();
    _violinGroups.clear();
    _groupCellIndices.clear();
    _violinPanels.clear();
    _pendingGeneDimensionIndices.clear();

    _nextGeneIndex                   = 0;
    _skippedGroupMembershipCount     = 0;
    _invalidExpressionValueCount     = 0;
    _negativeLogExpressionValueCount = 0;
    _geneLimitExceeded               = false;

    _violinPlotWidget->setData({}, {});
    _settingsAction.updatePlotOptions();
    _dropWidget->setShowDropIndicator(!_pointDataset.isValid());

    if (!_pointDataset.isValid()) {
        _settingsAction.getDatasetDisplayNameAction().setString("Violin plots — no expression data loaded");
        _violinPlotWidget->setMessage("Drop an expression Points dataset here.");
        _settingsAction.getStatusMessageAction().setString("No expression data loaded.");
        return;
    }

    if (_clusterDataset.isValid() && !isCompatibleClusterDataset(_clusterDataset)) {
        _violinPlotWidget->setMessage("The grouping no longer matches this expression dataset. Clear Cell groups or load compatible Clusters.");
        _settingsAction.getStatusMessageAction().setString("Incompatible cluster grouping.");
        return;
    }

    _settingsAction.getDatasetDisplayNameAction().setString(_pointDataset->getGuiName() + "  /  " + (_clusterDataset.isValid() ? _clusterDataset->getGuiName() : "All cells"));

    for (const auto geneDimensionIndex : _settingsAction.getGeneSelectionAction().getSelectedDimensions())
        _pendingGeneDimensionIndices.push_back(geneDimensionIndex);
    _geneLimitExceeded = _pendingGeneDimensionIndices.size() > violin::GeneSelectionAction::maximumDisplayedGenes;

    if (_geneLimitExceeded)
        _pendingGeneDimensionIndices.resize(violin::GeneSelectionAction::maximumDisplayedGenes);

    if (_pendingGeneDimensionIndices.empty()) {
        _violinPlotWidget->setMessage("Select one or more genes in the list.");
        _settingsAction.getStatusMessageAction().setString("No genes selected.");
        return;
    }

    if (_pointDataset->getNumPoints() > std::numeric_limits<std::uint32_t>::max()) {
        _violinPlotWidget->setMessage("This dataset exceeds the SDK's 32-bit cell-index range.");
        _settingsAction.getStatusMessageAction().setString("Too many cells for this view.");
        return;
    }

    const auto numberOfCells = static_cast<std::size_t>(_pointDataset->getNumPoints());

    if (_clusterDataset.isValid()) {
        std::vector<std::uint32_t> globalCellIndices;

        _pointDataset->getGlobalIndices(globalCellIndices);

        std::unordered_map<std::uint32_t, std::uint32_t> globalToLocalCellIndices;

        globalToLocalCellIndices.reserve(globalCellIndices.size());

        for (std::size_t localCellIndex = 0; localCellIndex < globalCellIndices.size(); ++localCellIndex)
            globalToLocalCellIndices.emplace(globalCellIndices[localCellIndex], static_cast<std::uint32_t>(localCellIndex));

        const auto& clusters = _clusterDataset->getClusters();

        for (const auto clusterIndex : _settingsAction.getCellTypeSelectionAction().getVisibleClusterIndices())
        {
            if (clusterIndex < 0 || clusterIndex >= clusters.size())
                continue;

            const auto& cluster = clusters[clusterIndex];

            _violinGroups.push_back({cluster.getName(), cluster.getColor(), clusterIndex});
            _groupCellIndices.emplace_back();

            auto& groupCellIndices = _groupCellIndices.back();

            groupCellIndices.reserve(cluster.getIndices().size());

            for (const auto globalCellIndex : cluster.getIndices())
            {
                const auto localCellIndexIterator = globalToLocalCellIndices.find(globalCellIndex);

                if (localCellIndexIterator != globalToLocalCellIndices.end() && localCellIndexIterator->second < numberOfCells)
                    groupCellIndices.push_back(localCellIndexIterator->second);
                else
                    ++_skippedGroupMembershipCount;
            }

            std::sort(groupCellIndices.begin(), groupCellIndices.end());
            groupCellIndices.erase(std::unique(groupCellIndices.begin(), groupCellIndices.end()), groupCellIndices.end());
        }
    } else {
        _violinGroups.push_back({"All cells", QColor("#7299cf"), -1});
        _groupCellIndices.emplace_back(numberOfCells);
        std::iota(_groupCellIndices.front().begin(), _groupCellIndices.front().end(), 0u);
    }

    if (_violinGroups.empty()) {
        const bool hasClusters = _clusterDataset.isValid() && !_clusterDataset->getClusters().isEmpty();

        _violinPlotWidget->setMessage(hasClusters ? "Check one or more cell types to show their violins, or choose Show all." : "The Clusters dataset has no groups. Clear Cell groups to plot without grouping.");
        _settingsAction.getStatusMessageAction().setString(hasClusters ? "No cell types are checked." : "No clusters available.");
        return;
    }

    synchronizeClusterSelection();
    _settingsAction.getStatusMessageAction().setString("Calculating expression distributions…");
    _violinPlotWidget->setMessage("Calculating expression distributions…");
    _geneCalculationTimer.start();
}

void ViolinPlotPlugin::calculateNextGeneDistribution()
{
    if (!_pointDataset.isValid() || _nextGeneIndex >= _pendingGeneDimensionIndices.size()) {
        _geneCalculationTimer.stop();
        return;
    }

    try {
        const int geneDimensionIndex = _pendingGeneDimensionIndices[_nextGeneIndex];

        if (geneDimensionIndex >= _pointDataset->getNumDimensions()) {
            updateGeneSelection();
            return;
        }

        std::vector<float> expressionValues;

        _pointDataset->extractDataForDimension(expressionValues, geneDimensionIndex);

        if (expressionValues.size() != static_cast<std::size_t>(_pointDataset->getNumPoints()))
            throw std::runtime_error("Expression dimension length does not match the loaded cell count.");

        const auto& geneDimensionNames = _pointDataset->getDimensionNames();
        ViolinPanel genePanel;

        genePanel.name = geneDimensionIndex < int(geneDimensionNames.size()) ? geneDimensionNames[geneDimensionIndex] : QString("Feature %1").arg(geneDimensionIndex + 1);

        const auto plotOptions = _settingsAction.getPlotOptions();

        for (const auto& groupCellIndices : _groupCellIndices)
        {
            std::vector<float> groupExpressionValues;

            groupExpressionValues.reserve(groupCellIndices.size());

            for (const auto localCellIndex : groupCellIndices)
            {
                if (localCellIndex < expressionValues.size())
                    groupExpressionValues.push_back(expressionValues[localCellIndex]);
            }

            auto distribution = violin::summarize(groupExpressionValues, plotOptions);

            _invalidExpressionValueCount     += distribution.invalid;
            _negativeLogExpressionValueCount += distribution.negativeLogValues;
            genePanel.distributions.push_back(std::move(distribution));
        }

        _violinPanels.push_back(std::move(genePanel));
        ++_nextGeneIndex;
        _violinPlotWidget->setData(_violinGroups, _violinPanels);

        if (_nextGeneIndex < _pendingGeneDimensionIndices.size()) {
            _settingsAction.getStatusMessageAction().setString(QString("Calculated %1 / %2 genes…").arg(qulonglong(_nextGeneIndex)).arg(qulonglong(_pendingGeneDimensionIndices.size())));
            return;
        }

        _geneCalculationTimer.stop();
        _settingsAction.setExportEnabled(true);

        QString statusMessage = QString("%1 genes · %2 groups · %3 cells/spots · widths normalized per violin · median / IQR").arg(qulonglong(_violinPanels.size())).arg(qulonglong(_violinGroups.size())).arg(qulonglong(_pointDataset->getNumPoints()));

        if (_geneLimitExceeded)
            statusMessage += QString(" · showing the first %1 selected genes").arg(qulonglong(violin::GeneSelectionAction::maximumDisplayedGenes));

        if (_skippedGroupMembershipCount)
            statusMessage += QString(" · %1 group memberships outside loaded cells").arg(qulonglong(_skippedGroupMembershipCount));

        if (_invalidExpressionValueCount)
            statusMessage += QString(" · %1 nonfinite values skipped").arg(qulonglong(_invalidExpressionValueCount));

        if (_negativeLogExpressionValueCount)
            statusMessage += QString(" · %1 negative values excluded for log1p").arg(qulonglong(_negativeLogExpressionValueCount));

        if (!_groupingWarningMessage.isEmpty())
            statusMessage += " · " + _groupingWarningMessage;

        _settingsAction.getStatusMessageAction().setString(statusMessage);
    } catch (const std::exception& error) {
        _geneCalculationTimer.stop();
        _violinPlotWidget->setData({}, {});
        _violinPlotWidget->setMessage("Could not calculate the expression distributions.");
        _settingsAction.getStatusMessageAction().setString(QString("Calculation failed: %1").arg(QString::fromUtf8(error.what())));
    }
}

void ViolinPlotPlugin::synchronizeClusterSelection()
{
    if (_violinPlotWidget == nullptr)
        return;

    QSet<int> selectedClusterIndices;

    if (_clusterDataset.isValid()) {
        for (const auto clusterIndex : _clusterDataset->getSelectionIndices())
        {
            if (clusterIndex < std::uint32_t(_clusterDataset->getClusters().size()))
                selectedClusterIndices.insert(int(clusterIndex));
        }
    }

    _violinPlotWidget->setSelectedClusters(selectedClusterIndices);
}

void ViolinPlotPlugin::activateClusterGroup(int clusterIndex, Qt::KeyboardModifiers modifiers)
{
    if (!_clusterDataset.isValid() || clusterIndex < 0 || clusterIndex >= _clusterDataset->getClusters().size())
        return;

    auto selectedClusterIndices = _clusterDataset->getSelectionIndices();

    if (modifiers.testFlag(Qt::ControlModifier)) {
        const auto selectedClusterIterator = std::find(selectedClusterIndices.begin(), selectedClusterIndices.end(), std::uint32_t(clusterIndex));

        if (selectedClusterIterator == selectedClusterIndices.end())
            selectedClusterIndices.push_back(std::uint32_t(clusterIndex));
        else
            selectedClusterIndices.erase(selectedClusterIterator);
    } else {
        selectedClusterIndices = {std::uint32_t(clusterIndex)};
    }

    _clusterDataset->setSelectionIndices(selectedClusterIndices);
    events().notifyDatasetDataSelectionChanged(_clusterDataset);
    synchronizeClusterSelection();
}

void ViolinPlotPlugin::fromVariantMap(const QVariantMap& variantMap)
{
    if (_violinPlotWidget == nullptr) {
        _deferredPluginState    = variantMap;
        _hasDeferredPluginState = true;
        return;
    }

    ViewPlugin::fromVariantMap(variantMap);

    _settingsAction.fromParentVariantMap(variantMap);

    schedulePlotUpdate();
}

QVariantMap ViolinPlotPlugin::toVariantMap() const
{
    auto variantMap = ViewPlugin::toVariantMap();

    _settingsAction.insertIntoVariantMap(variantMap);

    return variantMap;
}

ViolinPlotPluginFactory::ViolinPlotPluginFactory()
{
    setIconByName("chart-bar");
}

ViewPlugin* ViolinPlotPluginFactory::produce()
{
    return new ViolinPlotPlugin(this);
}

DataTypes ViolinPlotPluginFactory::supportedDataTypes() const
{
    return {PointType, ClusterType};
}

PluginTriggerActions ViolinPlotPluginFactory::getPluginTriggerActions(const Datasets& datasets) const
{
    if (datasets.isEmpty() || datasets.size() > 2)
        return {};

    bool hasPointDataset   = false;
    bool hasClusterDataset = false;

    for (const auto& dataset : datasets)
    {
        if (!dataset.isValid())
            return {};

        if (dataset->getDataType() == PointType && !hasPointDataset)
            hasPointDataset = true;
        else if (dataset->getDataType() == ClusterType && !hasClusterDataset)
            hasClusterDataset = true;
        else
            return {};
    }

    auto* pluginTriggerAction = new PluginTriggerAction(const_cast<ViolinPlotPluginFactory*>(this), this, "Violin plots", "View expression distributions by cell type or spatial region", icon(), [this, datasets](PluginTriggerAction&) {
        auto* violinPlotPlugin = dynamic_cast<ViolinPlotPlugin*>(plugins().requestViewPlugin(getKind()));

        if (violinPlotPlugin != nullptr)
            violinPlotPlugin->loadData(datasets);
    });

    return {pluginTriggerAction};
}
