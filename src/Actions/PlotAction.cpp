#include "PlotAction.h"

using namespace mv::gui;

namespace violin {

PlotAction::PlotAction(QObject* parent, const QString& title) :
    GroupAction(parent, title),
    _showCellDotsAction(this, "Show cell dots", true),
    _sharedYAxisAction(this, "Shared Y axis", false)
{
    setConnectionPermissionsToForceNone(true);
    setIconByName("sliders");
    setToolTip("Plot: sampled cell dots and shared expression axis");
    setConfigurationFlag(WidgetAction::ConfigurationFlag::ForceCollapsedInGroup);
    setLabelSizingType(LabelSizingType::Auto);
    _showCellDotsAction.setToolTip("Show up to 180 actual observations per violin using a stable reservoir sample. Density and statistics use every valid observation.");
    _sharedYAxisAction.setToolTip("Use the same expression range for all displayed genes.");

    addAction(&_showCellDotsAction, ToggleAction::CheckBox);
    addAction(&_sharedYAxisAction, ToggleAction::CheckBox);
    connect(&_showCellDotsAction, &ToggleAction::toggled, this, &PlotAction::displaySettingsChanged);
    connect(&_sharedYAxisAction, &ToggleAction::toggled, this, &PlotAction::displaySettingsChanged);
}

void PlotAction::fromVariantMap(const QVariantMap& variantMap)
{
    GroupAction::fromVariantMap(variantMap);
    _showCellDotsAction.fromParentVariantMap(variantMap);
    _sharedYAxisAction.fromParentVariantMap(variantMap);
}

QVariantMap PlotAction::toVariantMap() const
{
    auto variantMap = GroupAction::toVariantMap();

    _showCellDotsAction.insertIntoVariantMap(variantMap);
    _sharedYAxisAction.insertIntoVariantMap(variantMap);
    return variantMap;
}

}
