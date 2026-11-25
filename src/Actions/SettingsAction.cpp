#include "SettingsAction.h"

#include "../ViolinPlotPlugin.h"

#include <ClusterData/ClusterData.h>
#include <PointData/PointData.h>

#include <QLabel>
#include <QMessageBox>
#include <QScopedValueRollback>
#include <QSplitter>

using namespace mv;
using namespace mv::gui;

namespace violin {

SettingsAction::SettingsAction(QObject* parent, const QString& title) :
    GroupAction(parent, title),
    _datasetsAction(this, "Datasets"),
    _distributionAction(this, "Distribution"),
    _plotAction(this, "Plot"),
    _exportAction(this, "Export"),
    _geneSelectionAction(this, "Gene selection"),
    _cellTypeSelectionAction(this, "Cell type selection"),
    _helpAction(this, "Help"),
    _datasetDisplayNameAction(this, "Dataset", "Violin plots — no expression data loaded"),
    _statusMessageAction(this, "Status", "Drop expression Points data, then its Clusters data to compare cell types or spatial regions."),
    _primaryToolbarAction(this, "Primary toolbar")
{
    setConnectionPermissionsToForceNone(true);
    _helpAction.setIconByName("circle-question");
    _helpAction.setToolTip("Help: violin plot settings and interpretation");
    _datasetDisplayNameAction.setTextElideMode(Qt::ElideRight);
    _statusMessageAction.setTextElideMode(Qt::ElideNone);

    _primaryToolbarAction.addAction(&_datasetsAction);
    _primaryToolbarAction.addAction(&_distributionAction);
    _primaryToolbarAction.addAction(&_plotAction);
    _primaryToolbarAction.addAction(&_exportAction);
    _primaryToolbarAction.addAction(&_helpAction, 100, TriggerAction::Icon);

    addAction(&_datasetsAction);
    addAction(&_distributionAction);
    addAction(&_plotAction);
    addAction(&_exportAction);
    addAction(&_geneSelectionAction);
    addAction(&_cellTypeSelectionAction);
    addAction(&_helpAction, TriggerAction::Icon);
    connect(&_helpAction, &TriggerAction::triggered, this, &SettingsAction::showHelp);
}

QWidget* SettingsAction::getWidget(QWidget* parent, const std::int32_t& widgetFlags)
{
    Q_UNUSED(widgetFlags)
    return _primaryToolbarAction.createWidget(parent);
}

void SettingsAction::initialize(ViolinPlotPlugin* violinPlotPlugin)
{
    Q_ASSERT(violinPlotPlugin != nullptr);

    if (violinPlotPlugin == nullptr || _violinPlotPlugin != nullptr)
        return;

    _violinPlotPlugin = violinPlotPlugin;
    _exportAction.initialize(violinPlotPlugin->_violinPlotWidget, &violinPlotPlugin->getWidget());
    connect(&_datasetsAction.getPointDatasetPickerAction(), &DatasetPickerAction::currentDatasetChanged, this, [this](const Dataset<>& dataset) {
        if (!_isRestoringState && !_violinPlotPlugin->_isSynchronizingDatasetPickers)
            _violinPlotPlugin->setPointDataset(Dataset<Points>(dataset));
    });
    connect(&_datasetsAction.getClusterDatasetPickerAction(), &DatasetPickerAction::currentDatasetChanged, this, [this](const Dataset<>& dataset) {
        if (!_isRestoringState && !_violinPlotPlugin->_isSynchronizingDatasetPickers)
            _violinPlotPlugin->setClusterDataset(Dataset<Clusters>(dataset));
    });
    connect(&_datasetsAction, &DatasetsAction::calculationSettingsChanged, violinPlotPlugin, &ViolinPlotPlugin::schedulePlotUpdate);
    connect(&_distributionAction, &DistributionAction::calculationSettingsChanged, violinPlotPlugin, &ViolinPlotPlugin::schedulePlotUpdate);
    connect(&_plotAction, &PlotAction::displaySettingsChanged, this, &SettingsAction::updatePlotOptions);
    connect(&_geneSelectionAction, &GeneSelectionAction::selectionChanged, violinPlotPlugin, &ViolinPlotPlugin::schedulePlotUpdate);
    connect(&_cellTypeSelectionAction, &CellTypeSelectionAction::displayChanged, violinPlotPlugin, &ViolinPlotPlugin::schedulePlotUpdate);
    connect(violinPlotPlugin->_violinPlotWidget, &ViolinPlotWidget::renderFailed, this, [this](const QString& error) {
        _statusMessageAction.setString("D3 chart error: " + error);
        setExportEnabled(false);
    });
}

QWidget* SettingsAction::createTitleWidget(QWidget* parent)
{
    return _datasetDisplayNameAction.createWidget(parent, StringAction::Label, [](WidgetAction*, QWidget* widget) {
        if (auto* titleLabel = qobject_cast<QLabel*>(widget))
            titleLabel->setTextFormat(Qt::PlainText);
    });
}

QWidget* SettingsAction::createSelectionWidget(QWidget* parent)
{
    auto* selectionSplitter = new QSplitter(Qt::Vertical, parent);

    selectionSplitter->addWidget(_geneSelectionAction.createWidget(selectionSplitter, GroupAction::Vertical | GroupAction::NoMargins));
    selectionSplitter->addWidget(_cellTypeSelectionAction.createWidget(selectionSplitter));
    selectionSplitter->setChildrenCollapsible(false);
    selectionSplitter->setStretchFactor(0, 3);
    selectionSplitter->setStretchFactor(1, 2);
    selectionSplitter->setSizes({330, 220});
    return selectionSplitter;
}

QWidget* SettingsAction::createStatusWidget(QWidget* parent)
{
    return _statusMessageAction.createWidget(parent, StringAction::Label, [](WidgetAction*, QWidget* widget) {
        if (auto* statusLabel = qobject_cast<QLabel*>(widget)) {
            statusLabel->setTextFormat(Qt::PlainText);
            statusLabel->setWordWrap(true);
        }
    });
}

void SettingsAction::updatePlotOptions()
{
    if (_violinPlotPlugin == nullptr || _violinPlotPlugin->_violinPlotWidget == nullptr)
        return;

    _violinPlotPlugin->_violinPlotWidget->setDisplayOptions(_plotAction.getSharedYAxisAction().isChecked(), _plotAction.getShowCellDotsAction().isChecked(), _datasetsAction.getLogTransformAction().isChecked(), _distributionAction.getIncludeZerosAction().isChecked());
}

void SettingsAction::setExportEnabled(bool enabled)
{
    _exportAction.setExportEnabled(enabled);
}

Options SettingsAction::getPlotOptions() const
{
    return {_datasetsAction.getLogTransformAction().isChecked(), _distributionAction.getIncludeZerosAction().isChecked(), _distributionAction.getBandwidthScaleAction().getValue()};
}

void SettingsAction::loadDatasets()
{
    if (_violinPlotPlugin == nullptr)
        return;

    // Capture both selections before loading expression data can clear a grouping.
    Datasets datasets;
    const auto pointDataset   = _datasetsAction.getPointDatasetPickerAction().getCurrentDataset();
    const auto clusterDataset = _datasetsAction.getClusterDatasetPickerAction().getCurrentDataset();

    if (pointDataset.isValid())
        datasets.push_back(pointDataset);

    if (clusterDataset.isValid())
        datasets.push_back(clusterDataset);

    if (datasets.isEmpty())
        _violinPlotPlugin->setPointDataset({});
    else
        _violinPlotPlugin->loadData(datasets);

    if (!clusterDataset.isValid())
        _violinPlotPlugin->setClusterDataset({});
}

void SettingsAction::fromVariantMap(const QVariantMap& variantMap)
{
    const QScopedValueRollback<bool> restoringStateGuard(_isRestoringState, true);

    GroupAction::fromVariantMap(variantMap);
    _datasetsAction.fromParentVariantMap(variantMap);
    loadDatasets();
    _distributionAction.fromParentVariantMap(variantMap);
    _plotAction.fromParentVariantMap(variantMap);
    _geneSelectionAction.fromParentVariantMap(variantMap);
    _cellTypeSelectionAction.fromParentVariantMap(variantMap);
    updatePlotOptions();
}

QVariantMap SettingsAction::toVariantMap() const
{
    auto variantMap = GroupAction::toVariantMap();

    _datasetsAction.insertIntoVariantMap(variantMap);
    _distributionAction.insertIntoVariantMap(variantMap);
    _plotAction.insertIntoVariantMap(variantMap);
    _geneSelectionAction.insertIntoVariantMap(variantMap);
    _cellTypeSelectionAction.insertIntoVariantMap(variantMap);
    return variantMap;
}

void SettingsAction::showHelp()
{
    if (_violinPlotPlugin == nullptr)
        return;

    QMessageBox::information(&_violinPlotPlugin->getWidget(), "Violin plots",
        "The toolbar groups controls into Datasets (expression data, cell groups, and Apply log1p), Distribution (Include zeros and Smoothing), "
        "Plot (Show cell dots and Shared Y axis), Export (PNG/SVG), and Help.\n\n"
        "Load an expression Points dataset (cells/spots × genes), then optionally compatible Clusters. "
        "Clear Cell groups to plot all loaded cells without grouping. Dropdown selections and dataset drops update each other. "
        "Use gene checkboxes to select features. In Cell types, check groups to show them and drag rows to reorder them. "
        "Show all and Hide all change visibility without changing the cluster selection.\n\n"
        "Full symmetric KDE violins show each group in a separate panel for each gene. Density is trimmed to the observed range. "
        "White dots show medians, thick bars show the interquartile range, and thin lines show the observed range. Widths are normalized per violin; n is the number plotted.\n\n"
        "Cell dots are up to 180 actual observations per violin, sampled uniformly with a fixed seed. The plot-wide dot budget may reduce that limit. "
        "Density and summaries use all eligible observations.\n\n"
        "Zeros are included and log1p is off by default. Enable Apply log1p only for untransformed, nonnegative input; negative values are excluded and reported. "
        "Hover for detection rate (>0 among finite values), statistics, and skipped values.\n\n"
        "Click a group to select it in ManiVault; Ctrl-click toggles selection. PNG and SVG export all displayed panels. "
        "This view does not perform normalization or statistical testing.");
}

}
