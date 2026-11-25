#pragma once

#include <actions/GroupAction.h>
#include <actions/ToggleAction.h>

namespace violin {

/** Display controls that do not recalculate expression distributions. */
class PlotAction : public mv::gui::GroupAction
{
    Q_OBJECT

public:
    PlotAction(QObject* parent, const QString& title);

public: // Action getters
    mv::gui::ToggleAction& getShowCellDotsAction() { return _showCellDotsAction; }
    mv::gui::ToggleAction& getSharedYAxisAction() { return _sharedYAxisAction; }

public: // Serialization
    void fromVariantMap(const QVariantMap& variantMap) override;
    QVariantMap toVariantMap() const override;

signals:
    void displaySettingsChanged();

private:
    mv::gui::ToggleAction    _showCellDotsAction;
    mv::gui::ToggleAction    _sharedYAxisAction;
};

}
