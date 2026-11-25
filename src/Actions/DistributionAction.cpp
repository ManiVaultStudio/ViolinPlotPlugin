#include "DistributionAction.h"

using namespace mv::gui;

namespace violin {

DistributionAction::DistributionAction(QObject* parent, const QString& title) :
    GroupAction(parent, title),
    _includeZerosAction(this, "Include zeros", true),
    _bandwidthScaleAction(this, "Smoothing", 0.25f, 4.0f, 1.0f, 2)
{
    setConnectionPermissionsToForceNone(true);
    setIconByName("filter");
    setToolTip("Distribution: zero inclusion and KDE smoothing");
    setConfigurationFlag(WidgetAction::ConfigurationFlag::ForceCollapsedInGroup);
    setLabelSizingType(LabelSizingType::Auto);
    _includeZerosAction.setToolTip("Keep undetected cells in the distribution. Unchecking this plots only nonzero expression.");
    _bandwidthScaleAction.setToolTip("Multiplier for the automatic Gaussian KDE bandwidth. Violin widths are normalized independently.");
    _bandwidthScaleAction.setSingleStep(0.25f);
    _bandwidthScaleAction.setSuffix(" ×");

    addAction(&_includeZerosAction, ToggleAction::CheckBox);
    addAction(&_bandwidthScaleAction, DecimalAction::SpinBox);
    connect(&_includeZerosAction, &ToggleAction::toggled, this, &DistributionAction::calculationSettingsChanged);
    connect(&_bandwidthScaleAction, &DecimalAction::valueChanged, this, &DistributionAction::calculationSettingsChanged);
}

void DistributionAction::fromVariantMap(const QVariantMap& variantMap)
{
    GroupAction::fromVariantMap(variantMap);
    _includeZerosAction.fromParentVariantMap(variantMap);
    _bandwidthScaleAction.fromParentVariantMap(variantMap);
}

QVariantMap DistributionAction::toVariantMap() const
{
    auto variantMap = GroupAction::toVariantMap();

    _includeZerosAction.insertIntoVariantMap(variantMap);
    _bandwidthScaleAction.insertIntoVariantMap(variantMap);
    return variantMap;
}

}
