#pragma once

#include <actions/StringAction.h>
#include <actions/TriggerAction.h>
#include <actions/WidgetAction.h>

#include <QColor>
#include <vector>

class QAbstractItemModel;

namespace violin {

struct CellTypeEntry
{
    QString clusterId;
    int     clusterIndex = -1;
    QString name;
    QColor  color;
    bool    visible = true;
};

class CellTypeListModel;

/** Ordered cell-type visibility, independent of ManiVault's cluster selection. */
class CellTypeSelectionAction : public mv::gui::WidgetAction
{
    Q_OBJECT

public:
    CellTypeSelectionAction(QObject* parent, const QString& title);

    void setCellTypes(const QString& groupingDatasetId, const std::vector<CellTypeEntry>& cellTypes);
    std::vector<int> getVisibleClusterIndices() const;
    QAbstractItemModel& getCellTypeListModel();

public: // Action getters
    mv::gui::TriggerAction& getShowAllCellTypesAction() { return _showAllCellTypesAction; }
    mv::gui::TriggerAction& getHideAllCellTypesAction() { return _hideAllCellTypesAction; }

public: // Serialization
    void fromVariantMap(const QVariantMap& variantMap) override;
    QVariantMap toVariantMap() const override;

signals:
    void displayChanged();

protected:
    QWidget* getWidget(QWidget* parent, const std::int32_t& widgetFlags) override;

private:
    QVariantList getCellTypeState() const;
    void restoreCellTypeState(const QVariantList& cellTypeState);
    void setAllCellTypesVisible(bool visible);

    CellTypeListModel*          _cellTypeListModel;
    mv::gui::StringAction       _cellTypeHeadingAction;
    mv::gui::StringAction       _cellTypeHintAction;
    mv::gui::TriggerAction      _showAllCellTypesAction;
    mv::gui::TriggerAction      _hideAllCellTypesAction;
    QString                    _groupingDatasetId;
    QVariantList               _pendingCellTypeState;
    bool                       _hasPendingCellTypeState = false;
};

}
