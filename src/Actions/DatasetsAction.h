#pragma once

#include <actions/DatasetPickerAction.h>
#include <actions/GroupAction.h>
#include <actions/ToggleAction.h>

class Points;
class Clusters;

namespace violin {

/** Expression input, compatible cell grouping, and the optional log transform. */
class DatasetsAction : public mv::gui::GroupAction
{
    Q_OBJECT

public:
    DatasetsAction(QObject* parent, const QString& title);
    void updateClusterDatasetFilter(const mv::Dataset<Points>& pointDataset);
    static bool isCompatibleGrouping(const mv::Dataset<Points>& pointDataset, const mv::Dataset<Clusters>& clusterDataset);

public: // Action getters
    mv::gui::DatasetPickerAction& getPointDatasetPickerAction() { return _pointDatasetPickerAction; }
    mv::gui::DatasetPickerAction& getClusterDatasetPickerAction() { return _clusterDatasetPickerAction; }
    mv::gui::ToggleAction& getLogTransformAction() { return _logTransformAction; }
    const mv::gui::ToggleAction& getLogTransformAction() const { return _logTransformAction; }

public: // Serialization
    void fromVariantMap(const QVariantMap& variantMap) override;
    QVariantMap toVariantMap() const override;

signals:
    void calculationSettingsChanged();

private:
    mv::gui::DatasetPickerAction    _pointDatasetPickerAction;
    mv::gui::DatasetPickerAction    _clusterDatasetPickerAction;
    mv::gui::ToggleAction           _logTransformAction;
};

}
