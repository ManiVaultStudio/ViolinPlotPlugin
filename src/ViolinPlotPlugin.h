#pragma once

#include "Actions/SettingsAction.h"
#include "ViolinPlotWidget.h"

#include <Dataset.h>
#include <ViewPlugin.h>

#include <QTimer>

#include <cstdint>
#include <vector>

class Points;
class Clusters;

namespace mv::gui {
class DropWidget;
}

/** Gene-expression distributions grouped by cells, cell types, or regions. */
class ViolinPlotPlugin : public mv::plugin::ViewPlugin
{
    Q_OBJECT

public:
    explicit ViolinPlotPlugin(const mv::plugin::PluginFactory* pluginFactory);

    void init() override;
    void loadData(const mv::Datasets& datasets) override;

public: // Serialization
    void fromVariantMap(const QVariantMap& variantMap) override;
    QVariantMap toVariantMap() const override;

public: // Action getters
    violin::SettingsAction& getSettingsAction() { return _settingsAction; }

private:
    void setPointDataset(const mv::Dataset<Points>& pointDataset);
    void setClusterDataset(const mv::Dataset<Clusters>& clusterDataset);
    bool isCompatibleClusterDataset(const mv::Dataset<Clusters>& clusterDataset) const;
    void updateGeneSelection();
    void updateCellTypeSelection();
    void schedulePlotUpdate();
    void beginPlotUpdate();
    void calculateNextGeneDistribution();
    void synchronizeClusterSelection();
    void activateClusterGroup(int clusterIndex, Qt::KeyboardModifiers modifiers);

    mv::Dataset<Points>                        _pointDataset;
    mv::Dataset<Clusters>                      _clusterDataset;
    violin::SettingsAction                     _settingsAction;
    ViolinPlotWidget*                          _violinPlotWidget = nullptr;
    mv::gui::DropWidget*                       _dropWidget = nullptr;
    QTimer                                     _updateDebounceTimer;
    QTimer                                     _geneCalculationTimer;
    std::vector<int>                           _pendingGeneDimensionIndices;
    std::size_t                                _nextGeneIndex = 0;
    std::vector<ViolinGroup>                   _violinGroups;
    std::vector<std::vector<std::uint32_t>>    _groupCellIndices;
    std::vector<ViolinPanel>                   _violinPanels;
    std::size_t                                _skippedGroupMembershipCount = 0;
    std::size_t                                _invalidExpressionValueCount = 0;
    std::size_t                                _negativeLogExpressionValueCount = 0;
    bool                                       _geneLimitExceeded = false;
    mv::Datasets                               _deferredDatasets;
    QVariantMap                                _deferredPluginState;
    bool                                       _hasDeferredPluginState = false;
    bool                                       _isSynchronizingDatasetPickers = false;
    QString                                    _groupingWarningMessage;

    friend class violin::SettingsAction;
};

class ViolinPlotPluginFactory : public mv::plugin::ViewPluginFactory
{
    Q_OBJECT
    Q_INTERFACES(mv::plugin::ViewPluginFactory mv::plugin::PluginFactory)
    Q_PLUGIN_METADATA(IID "studio.manivault.ViolinPlotPlugin" FILE "PluginInfo.json")

public:
    ViolinPlotPluginFactory();

    mv::plugin::ViewPlugin* produce() override;
    mv::DataTypes supportedDataTypes() const override;
    mv::gui::PluginTriggerActions getPluginTriggerActions(const mv::Datasets& datasets) const override;
};
