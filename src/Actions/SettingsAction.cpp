#include "SettingsAction.h"

#include "../ViolinPlotPlugin.h"

#include <ClusterData/ClusterData.h>

#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QSplitter>

using namespace mv;
using namespace mv::gui;

namespace violin {

SettingsAction::SettingsAction(QObject* parent, const QString& title) :
    GroupAction(parent, title),
    _geneSelectionAction(this, "Gene selection"),
    _cellTypeSelectionAction(this, "Cell type selection"),
    _pointDatasetPickerAction(this, "Expression data"),
    _clusterDatasetPickerAction(this, "Grouping"),
    _logTransformAction(this, "log1p", false),
    _includeZerosAction(this, "Include zeros", true),
    _showCellDotsAction(this, "Cell dots", true),
    _sharedYAxisAction(this, "Shared Y axis", false),
    _bandwidthScaleAction(this, "Smooth", 0.25f, 4.0f, 1.0f, 2),
    _showAllCellsAction(this, "All cells"),
    _helpAction(this, "Help"),
    _exportPngAction(this, "PNG"),
    _exportSvgAction(this, "SVG"),
    _datasetDisplayNameAction(this, "Dataset", "Violin plots — no expression data loaded"),
    _statusMessageAction(this, "Status", "Drop expression Points data, then its Clusters data to compare cell types or spatial regions.")
{
    setConnectionPermissionsToForceNone(true);

    _logTransformAction.setToolTip("Natural log ln(1+x). Use for nonnegative expression that has not already been log-transformed. Negative values are excluded and reported.");
    _includeZerosAction.setToolTip("Keep undetected cells in the distribution. Unchecking this plots only nonzero expression.");
    _showCellDotsAction.setToolTip("Show up to 180 actual observations per violin using a stable reservoir sample. Density and statistics use every valid observation.");
    _sharedYAxisAction.setToolTip("Use the same expression range for all displayed genes.");
    _bandwidthScaleAction.setToolTip("Multiplier for the automatic Gaussian KDE bandwidth. Violin widths are normalized independently.");
    _bandwidthScaleAction.setSingleStep(0.25f);
    _bandwidthScaleAction.setPrefix("Smooth × ");
    _showAllCellsAction.setToolTip("Remove cluster grouping and plot the entire loaded dataset.");
    _helpAction.setToolTip("Explain the violin plot controls and summaries.");
    _exportPngAction.setToolTip("Export all displayed gene panels as a PNG image.");
    _exportSvgAction.setToolTip("Export all displayed gene panels as a vector SVG image.");
    _datasetDisplayNameAction.setTextElideMode(Qt::ElideRight);
    _statusMessageAction.setTextElideMode(Qt::ElideNone);

    addAction(&_logTransformAction, ToggleAction::CheckBox);
    addAction(&_includeZerosAction, ToggleAction::CheckBox);
    addAction(&_showCellDotsAction, ToggleAction::CheckBox);
    addAction(&_sharedYAxisAction, ToggleAction::CheckBox);
    addAction(&_bandwidthScaleAction, DecimalAction::SpinBox);
    addStretch();
    addAction(&_exportPngAction, TriggerAction::Text);
    addAction(&_exportSvgAction, TriggerAction::Text);

    setExportEnabled(false);
}

void SettingsAction::initialize(ViolinPlotPlugin* violinPlotPlugin)
{
    Q_ASSERT(violinPlotPlugin != nullptr);

    if (violinPlotPlugin == nullptr || _violinPlotPlugin != nullptr)
        return;

    _violinPlotPlugin = violinPlotPlugin;

    connect(&_geneSelectionAction, &GeneSelectionAction::selectionChanged, violinPlotPlugin, &ViolinPlotPlugin::schedulePlotUpdate);
    connect(&_cellTypeSelectionAction, &CellTypeSelectionAction::displayChanged, violinPlotPlugin, &ViolinPlotPlugin::schedulePlotUpdate);
    connect(&_logTransformAction, &ToggleAction::toggled, violinPlotPlugin, &ViolinPlotPlugin::schedulePlotUpdate);
    connect(&_includeZerosAction, &ToggleAction::toggled, violinPlotPlugin, &ViolinPlotPlugin::schedulePlotUpdate);
    connect(&_bandwidthScaleAction, &DecimalAction::valueChanged, violinPlotPlugin, &ViolinPlotPlugin::schedulePlotUpdate);
    connect(&_showCellDotsAction, &ToggleAction::toggled, this, &SettingsAction::updatePlotOptions);
    connect(&_sharedYAxisAction, &ToggleAction::toggled, this, &SettingsAction::updatePlotOptions);
    connect(&_showAllCellsAction, &TriggerAction::triggered, this, [this] {
        _violinPlotPlugin->setClusterDataset({});
    });
    connect(&_exportPngAction, &TriggerAction::triggered, this, [this] {
        _violinPlotPlugin->exportPlot(false);
    });
    connect(&_exportSvgAction, &TriggerAction::triggered, this, [this] {
        _violinPlotPlugin->exportPlot(true);
    });
    connect(&_helpAction, &TriggerAction::triggered, this, &SettingsAction::showHelp);
    connect(_violinPlotPlugin->_violinPlotWidget, &ViolinPlotWidget::exportFinished, this, [this](bool success, const QString& error) {
        if (!success)
            QMessageBox::warning(&_violinPlotPlugin->getWidget(), "Export failed", error);
    });
    connect(_violinPlotPlugin->_violinPlotWidget, &ViolinPlotWidget::renderFailed, this, [this](const QString& error) {
        _statusMessageAction.setString("D3 chart error: " + error);
        setExportEnabled(false);
    });
}

QWidget* SettingsAction::createDatasetWidget(QWidget* parent)
{
    auto* widget = new QWidget(parent);
    auto* layout = new QHBoxLayout(widget);

    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(_datasetDisplayNameAction.createWidget(widget, StringAction::Label, [](WidgetAction*, QWidget* labelWidget) {
        if (auto* label = qobject_cast<QLabel*>(labelWidget))
            label->setTextFormat(Qt::PlainText);
    }), 1);
    layout->addWidget(_showAllCellsAction.createWidget(widget, TriggerAction::Text));
    layout->addWidget(_helpAction.createWidget(widget, TriggerAction::Text));

    return widget;
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
        if (auto* label = qobject_cast<QLabel*>(widget)) {
            label->setTextFormat(Qt::PlainText);
            label->setWordWrap(true);
        }
    });
}

void SettingsAction::updatePlotOptions()
{
    if (_violinPlotPlugin == nullptr || _violinPlotPlugin->_violinPlotWidget == nullptr)
        return;

    _violinPlotPlugin->_violinPlotWidget->setDisplayOptions(_sharedYAxisAction.isChecked(), _showCellDotsAction.isChecked(), _logTransformAction.isChecked(), _includeZerosAction.isChecked());
}

void SettingsAction::setExportEnabled(bool enabled)
{
    _exportPngAction.setEnabled(enabled);
    _exportSvgAction.setEnabled(enabled);
}

Options SettingsAction::getPlotOptions() const
{
    return {_logTransformAction.isChecked(), _includeZerosAction.isChecked(), _bandwidthScaleAction.getValue()};
}

void SettingsAction::loadDatasets()
{
    if (_violinPlotPlugin == nullptr)
        return;

    // Capture both before loading points, which may clear an old grouping.
    Datasets datasets;

    if (_pointDatasetPickerAction.hasSelection())
        datasets.push_back(_pointDatasetPickerAction.getCurrentDataset());

    if (_clusterDatasetPickerAction.hasSelection())
        datasets.push_back(_clusterDatasetPickerAction.getCurrentDataset());

    _violinPlotPlugin->loadData(datasets);
}

void SettingsAction::fromVariantMap(const QVariantMap& variantMap)
{
    GroupAction::fromVariantMap(variantMap);

    _pointDatasetPickerAction.fromParentVariantMap(variantMap);
    _clusterDatasetPickerAction.fromParentVariantMap(variantMap);
    loadDatasets();

    _logTransformAction.fromParentVariantMap(variantMap);
    _includeZerosAction.fromParentVariantMap(variantMap);
    _showCellDotsAction.fromParentVariantMap(variantMap);
    _sharedYAxisAction.fromParentVariantMap(variantMap);
    _bandwidthScaleAction.fromParentVariantMap(variantMap);
    _geneSelectionAction.fromParentVariantMap(variantMap);
    _cellTypeSelectionAction.fromParentVariantMap(variantMap);

    updatePlotOptions();
}

QVariantMap SettingsAction::toVariantMap() const
{
    auto variantMap = GroupAction::toVariantMap();

    _pointDatasetPickerAction.insertIntoVariantMap(variantMap);
    _clusterDatasetPickerAction.insertIntoVariantMap(variantMap);
    _logTransformAction.insertIntoVariantMap(variantMap);
    _includeZerosAction.insertIntoVariantMap(variantMap);
    _showCellDotsAction.insertIntoVariantMap(variantMap);
    _sharedYAxisAction.insertIntoVariantMap(variantMap);
    _bandwidthScaleAction.insertIntoVariantMap(variantMap);
    _geneSelectionAction.insertIntoVariantMap(variantMap);
    _cellTypeSelectionAction.insertIntoVariantMap(variantMap);

    return variantMap;
}

void SettingsAction::showHelp()
{
    if (_violinPlotPlugin == nullptr)
        return;

    QMessageBox::information(&_violinPlotPlugin->getWidget(), "Violin plots",
        "Load an expression Points dataset (cells/spots × genes), then optionally its Clusters dataset. "
        "Use the gene checkboxes to select features. In the Cell types panel, check groups to show them and drag rows to reorder them. "
        "All cell types are shown by default; Show all and Hide all change visibility without changing the cluster selection.\n\n"
        "Full symmetric KDE violins show each group in a separate panel for each gene. "
        "Density is trimmed to the observed range. White dots show medians, thick bars show the interquartile range, "
        "and thin lines show the observed range. Widths are normalized per violin; n is the number plotted.\n\n"
        "Cell dots are up to 180 actual observations per violin, sampled uniformly with a fixed seed. "
        "The plot-wide dot budget may reduce that limit. Density and summaries use all eligible observations.\n\n"
        "Zeros are included and log1p is off by default. Enable log1p only for untransformed, nonnegative input; "
        "negative values are excluded and reported. Hover for detection rate (>0 among finite values), statistics, and skipped values.\n\n"
        "Click a group to select it in ManiVault; Ctrl-click toggles selection. PNG and SVG export all displayed panels. "
        "This view does not perform normalization or statistical testing.");
}

}
