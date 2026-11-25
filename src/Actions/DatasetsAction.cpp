#include "DatasetsAction.h"

#include <ClusterData/ClusterData.h>
#include <PointData/PointData.h>

using namespace mv;
using namespace mv::gui;

namespace violin {

DatasetsAction::DatasetsAction(QObject* parent, const QString& title) :
    GroupAction(parent, title),
    _pointDatasetPickerAction(this, "Expression"),
    _clusterDatasetPickerAction(this, "Groups"),
    _logTransformAction(this, "Apply log1p", false)
{
    setConnectionPermissionsToForceNone(true);
    setIconByName("database");
    setToolTip("Datasets: expression data, cell groups, and input transform");
    setDefaultWidgetFlags(GroupAction::Horizontal);
    setLabelSizingType(LabelSizingType::Auto);

    _pointDatasetPickerAction.setFilterFunction([](const Dataset<>& dataset) {
        return dataset.isValid() && dataset->getDataType() == PointType;
    });
    _clusterDatasetPickerAction.setDefaultWidgetFlag(OptionAction::Clearable);
    _pointDatasetPickerAction.setToolTip("Expression Points: cells/spots as rows and genes as dimensions.");
    _clusterDatasetPickerAction.setToolTip("Choose compatible cell groups. Clear this selection to plot all loaded cells without grouping.");
    _logTransformAction.setToolTip("Natural log ln(1+x). Use for nonnegative expression that has not already been log-transformed. Negative values are excluded and reported.");
    updateClusterDatasetFilter({});

    addAction(&_pointDatasetPickerAction);
    addAction(&_clusterDatasetPickerAction);
    addAction(&_logTransformAction, ToggleAction::CheckBox);
    connect(&_logTransformAction, &ToggleAction::toggled, this, &DatasetsAction::calculationSettingsChanged);
}

bool DatasetsAction::isCompatibleGrouping(const Dataset<Points>& pointDataset, const Dataset<Clusters>& clusterDataset)
{
    if (!pointDataset.isValid() || !clusterDataset.isValid())
        return false;

    const auto parentDataset = clusterDataset->getParent();

    if (!parentDataset.isValid() || parentDataset->getDataType() != PointType)
        return false;

    const Dataset<Points> clusterPointDataset(parentDataset);
    const auto expressionSourceDataset = pointDataset->getSourceDataset<Points>();
    const auto clusterSourceDataset    = clusterPointDataset->getSourceDataset<Points>();

    return expressionSourceDataset.isValid() && clusterSourceDataset.isValid() && expressionSourceDataset->getId() == clusterSourceDataset->getId();
}

void DatasetsAction::updateClusterDatasetFilter(const Dataset<Points>& pointDataset)
{
    _clusterDatasetPickerAction.setFilterFunction([pointDataset](const Dataset<>& dataset) {
        if (!dataset.isValid() || dataset->getDataType() != ClusterType)
            return false;

        const Dataset<Clusters> clusterDataset(dataset);
        const auto parentDataset = clusterDataset->getParent();

        return parentDataset.isValid() && parentDataset->getDataType() == PointType && (!pointDataset.isValid() || isCompatibleGrouping(pointDataset, clusterDataset));
    });
}

void DatasetsAction::fromVariantMap(const QVariantMap& variantMap)
{
    GroupAction::fromVariantMap(variantMap);
    _pointDatasetPickerAction.fromParentVariantMap(variantMap);
    updateClusterDatasetFilter(Dataset<Points>(_pointDatasetPickerAction.getCurrentDataset()));
    _clusterDatasetPickerAction.fromParentVariantMap(variantMap);
    _logTransformAction.fromParentVariantMap(variantMap);
}

QVariantMap DatasetsAction::toVariantMap() const
{
    auto variantMap = GroupAction::toVariantMap();

    _pointDatasetPickerAction.insertIntoVariantMap(variantMap);
    _clusterDatasetPickerAction.insertIntoVariantMap(variantMap);
    _logTransformAction.insertIntoVariantMap(variantMap);
    return variantMap;
}

}
