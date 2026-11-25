#pragma once

#include <actions/GroupAction.h>
#include <actions/TriggerAction.h>

#include <QPointer>

class ViolinPlotWidget;

namespace violin {

/** PNG/SVG export controls and file dialogs. */
class ExportAction : public mv::gui::GroupAction
{
    Q_OBJECT

public:
    ExportAction(QObject* parent, const QString& title);
    void initialize(ViolinPlotWidget* violinPlotWidget, QWidget* dialogParentWidget);
    void setExportEnabled(bool enabled);

private:
    void savePlot(bool exportAsSvg);

    mv::gui::TriggerAction        _exportPngAction;
    mv::gui::TriggerAction        _exportSvgAction;
    QPointer<ViolinPlotWidget>    _violinPlotWidget;
    QPointer<QWidget>             _dialogParentWidget;
};

}
