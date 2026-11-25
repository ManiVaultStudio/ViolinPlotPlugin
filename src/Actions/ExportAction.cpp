#include "ExportAction.h"

#include "../ViolinPlotWidget.h"

#include <QFileDialog>
#include <QMessageBox>

using namespace mv::gui;

namespace violin {

ExportAction::ExportAction(QObject* parent, const QString& title) :
    GroupAction(parent, title),
    _exportPngAction(this, "Save PNG"),
    _exportSvgAction(this, "Save SVG")
{
    setConnectionPermissionsToForceNone(true);
    setIconByName("download");
    setToolTip("Export: save all displayed gene panels as PNG or SVG");
    setConfigurationFlag(WidgetAction::ConfigurationFlag::ForceCollapsedInGroup);
    setShowLabels(false);
    _exportPngAction.setToolTip("Export all displayed gene panels as a PNG image.");
    _exportSvgAction.setToolTip("Export all displayed gene panels as a vector SVG image.");
    addAction(&_exportPngAction, TriggerAction::Text);
    addAction(&_exportSvgAction, TriggerAction::Text);
    connect(&_exportPngAction, &TriggerAction::triggered, this, [this] {
        savePlot(false);
    });
    connect(&_exportSvgAction, &TriggerAction::triggered, this, [this] {
        savePlot(true);
    });
    setExportEnabled(false);
}

void ExportAction::initialize(ViolinPlotWidget* violinPlotWidget, QWidget* dialogParentWidget)
{
    _violinPlotWidget   = violinPlotWidget;
    _dialogParentWidget = dialogParentWidget;
    connect(violinPlotWidget, &ViolinPlotWidget::exportFinished, this, [this](bool success, const QString& error) {
        if (!success)
            QMessageBox::warning(_dialogParentWidget, "Export failed", error);
    });
}

void ExportAction::setExportEnabled(bool enabled)
{
    setEnabled(enabled);
    _exportPngAction.setEnabled(enabled);
    _exportSvgAction.setEnabled(enabled);
}

void ExportAction::savePlot(bool exportAsSvg)
{
    if (_violinPlotWidget == nullptr)
        return;

    QString fileName = QFileDialog::getSaveFileName(_dialogParentWidget, "Export violin plots", exportAsSvg ? "violin-plots.svg" : "violin-plots.png", exportAsSvg ? "SVG image (*.svg)" : "PNG image (*.png)");

    if (fileName.isEmpty())
        return;

    const QString suffix = exportAsSvg ? ".svg" : ".png";

    if (!fileName.endsWith(suffix, Qt::CaseInsensitive))
        fileName += suffix;

    _violinPlotWidget->exportPlot(fileName, exportAsSvg);
}

}
