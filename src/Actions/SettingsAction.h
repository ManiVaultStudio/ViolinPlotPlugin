#pragma once

#include "GeneSelectionAction.h"
#include "CellTypeSelectionAction.h"
#include "../ViolinStatistics.h"

#include <actions/DatasetPickerAction.h>
#include <actions/DecimalAction.h>
#include <actions/GroupAction.h>
#include <actions/StringAction.h>
#include <actions/ToggleAction.h>
#include <actions/TriggerAction.h>

class ViolinPlotPlugin;

namespace violin {

/** Owns violin plot controls, callbacks, and persistent settings. */
class SettingsAction : public mv::gui::GroupAction
{
    Q_OBJECT

public:
    SettingsAction(QObject* parent, const QString& title);

    void initialize(ViolinPlotPlugin* violinPlotPlugin);
    QWidget* createDatasetWidget(QWidget* parent);
    QWidget* createSelectionWidget(QWidget* parent);
    QWidget* createStatusWidget(QWidget* parent);
    void updatePlotOptions();
    void setExportEnabled(bool enabled);
    Options getPlotOptions() const;

public: // Serialization
    void fromVariantMap(const QVariantMap& variantMap) override;
    QVariantMap toVariantMap() const override;

public: // Action getters
    GeneSelectionAction& getGeneSelectionAction() { return _geneSelectionAction; }
    CellTypeSelectionAction& getCellTypeSelectionAction() { return _cellTypeSelectionAction; }
    mv::gui::DatasetPickerAction& getPointDatasetPickerAction() { return _pointDatasetPickerAction; }
    mv::gui::DatasetPickerAction& getClusterDatasetPickerAction() { return _clusterDatasetPickerAction; }
    mv::gui::ToggleAction& getLogTransformAction() { return _logTransformAction; }
    mv::gui::ToggleAction& getIncludeZerosAction() { return _includeZerosAction; }
    mv::gui::ToggleAction& getShowCellDotsAction() { return _showCellDotsAction; }
    mv::gui::ToggleAction& getSharedYAxisAction() { return _sharedYAxisAction; }
    mv::gui::DecimalAction& getBandwidthScaleAction() { return _bandwidthScaleAction; }
    mv::gui::StringAction& getDatasetDisplayNameAction() { return _datasetDisplayNameAction; }
    mv::gui::StringAction& getStatusMessageAction() { return _statusMessageAction; }

private:
    void loadDatasets();
    void showHelp();

    ViolinPlotPlugin*               _violinPlotPlugin = nullptr;
    GeneSelectionAction             _geneSelectionAction;
    CellTypeSelectionAction         _cellTypeSelectionAction;
    mv::gui::DatasetPickerAction    _pointDatasetPickerAction;
    mv::gui::DatasetPickerAction    _clusterDatasetPickerAction;
    mv::gui::ToggleAction           _logTransformAction;
    mv::gui::ToggleAction           _includeZerosAction;
    mv::gui::ToggleAction           _showCellDotsAction;
    mv::gui::ToggleAction           _sharedYAxisAction;
    mv::gui::DecimalAction          _bandwidthScaleAction;
    mv::gui::TriggerAction          _showAllCellsAction;
    mv::gui::TriggerAction          _helpAction;
    mv::gui::TriggerAction          _exportPngAction;
    mv::gui::TriggerAction          _exportSvgAction;
    mv::gui::StringAction           _datasetDisplayNameAction;
    mv::gui::StringAction           _statusMessageAction;
};

}
