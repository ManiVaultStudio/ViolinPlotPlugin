#pragma once

#include "CellTypeSelectionAction.h"
#include "DatasetsAction.h"
#include "DistributionAction.h"
#include "ExportAction.h"
#include "GeneSelectionAction.h"
#include "PlotAction.h"
#include "../ViolinStatistics.h"

#include <actions/GroupAction.h>
#include <actions/HorizontalToolbarAction.h>
#include <actions/StringAction.h>
#include <actions/TriggerAction.h>

class ViolinPlotPlugin;

namespace violin {

/** Coordinates the icon toolbar, selection panels, and persistent settings. */
class SettingsAction : public mv::gui::GroupAction
{
    Q_OBJECT

public:
    SettingsAction(QObject* parent, const QString& title);

    void initialize(ViolinPlotPlugin* violinPlotPlugin);
    QWidget* createSelectionWidget(QWidget* parent);
    QWidget* createStatusWidget(QWidget* parent);
    void updatePlotOptions();
    void setExportEnabled(bool enabled);
    Options getPlotOptions() const;

public: // Serialization
    void fromVariantMap(const QVariantMap& variantMap) override;
    QVariantMap toVariantMap() const override;

public: // Action getters
    DatasetsAction& getDatasetsAction() { return _datasetsAction; }
    DistributionAction& getDistributionAction() { return _distributionAction; }
    PlotAction& getPlotAction() { return _plotAction; }
    ExportAction& getExportAction() { return _exportAction; }
    GeneSelectionAction& getGeneSelectionAction() { return _geneSelectionAction; }
    CellTypeSelectionAction& getCellTypeSelectionAction() { return _cellTypeSelectionAction; }
    mv::gui::StringAction& getStatusMessageAction() { return _statusMessageAction; }

protected:
    QWidget* getWidget(QWidget* parent, const std::int32_t& widgetFlags) override;

private:
    void loadDatasets();
    void showHelp();

    ViolinPlotPlugin*                    _violinPlotPlugin = nullptr;
    DatasetsAction                       _datasetsAction;
    DistributionAction                   _distributionAction;
    PlotAction                           _plotAction;
    ExportAction                         _exportAction;
    GeneSelectionAction                  _geneSelectionAction;
    CellTypeSelectionAction              _cellTypeSelectionAction;
    mv::gui::TriggerAction               _helpAction;
    mv::gui::StringAction                _statusMessageAction;
    mv::gui::HorizontalToolbarAction     _primaryToolbarAction;
    bool                                 _isRestoringState = false;
};

}
