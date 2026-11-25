#pragma once

#include <actions/DecimalAction.h>
#include <actions/GroupAction.h>
#include <actions/ToggleAction.h>

namespace violin {

/** Observations included in each distribution and KDE smoothing. */
class DistributionAction : public mv::gui::GroupAction
{
    Q_OBJECT

public:
    DistributionAction(QObject* parent, const QString& title);

public: // Action getters
    mv::gui::ToggleAction& getIncludeZerosAction() { return _includeZerosAction; }
    const mv::gui::ToggleAction& getIncludeZerosAction() const { return _includeZerosAction; }
    mv::gui::DecimalAction& getBandwidthScaleAction() { return _bandwidthScaleAction; }
    const mv::gui::DecimalAction& getBandwidthScaleAction() const { return _bandwidthScaleAction; }

public: // Serialization
    void fromVariantMap(const QVariantMap& variantMap) override;
    QVariantMap toVariantMap() const override;

signals:
    void calculationSettingsChanged();

private:
    mv::gui::ToggleAction     _includeZerosAction;
    mv::gui::DecimalAction    _bandwidthScaleAction;
};

}
