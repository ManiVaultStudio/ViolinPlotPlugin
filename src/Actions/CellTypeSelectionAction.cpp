#include "CellTypeSelectionAction.h"

#include <QAbstractListModel>
#include <QDataStream>
#include <QHBoxLayout>
#include <QLabel>
#include <QListView>
#include <QMimeData>
#include <QUuid>
#include <QVBoxLayout>

#include <algorithm>
#include <utility>

using namespace mv::gui;

namespace violin {

/** Shared model keeps check states and row order consistent in every action widget. */
class CellTypeListModel : public QAbstractListModel
{
public:
    explicit CellTypeListModel(QObject* parent) : QAbstractListModel(parent) {}

    int rowCount(const QModelIndex& parent = {}) const override
    {
        return parent.isValid() ? 0 : static_cast<int>(_cellTypes.size());
    }

    QVariant data(const QModelIndex& index, int role) const override
    {
        if (!index.isValid() || index.row() < 0 || index.row() >= rowCount() || index.column() != 0)
            return {};

        const auto& cellType = _cellTypes[index.row()];

        switch (role) {
            case Qt::DisplayRole:
            case Qt::ToolTipRole:
                return cellType.name;

            case Qt::CheckStateRole:
                return cellType.visible ? Qt::Checked : Qt::Unchecked;

            case Qt::DecorationRole:
                return cellType.color;

            case Qt::UserRole:
                return cellType.clusterIndex;

            default:
                return {};
        }
    }

    Qt::ItemFlags flags(const QModelIndex& index) const override
    {
        if (!index.isValid())
            return Qt::ItemIsDropEnabled;

        return Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsUserCheckable | Qt::ItemIsDragEnabled;
    }

    bool setData(const QModelIndex& index, const QVariant& value, int role) override
    {
        if (role != Qt::CheckStateRole || !index.isValid() || index.row() < 0 || index.row() >= rowCount() || index.column() != 0)
            return false;

        const bool visible = value.toInt() == Qt::Checked;
        auto& cellType     = _cellTypes[index.row()];

        if (cellType.visible == visible)
            return false;

        cellType.visible = visible;
        emit dataChanged(index, index, {Qt::CheckStateRole});
        return true;
    }

    Qt::DropActions supportedDropActions() const override { return Qt::MoveAction; }
    Qt::DropActions supportedDragActions() const override { return Qt::MoveAction; }
    QStringList mimeTypes() const override { return {"application/x-manivault-violin-cell-type"}; }

    QMimeData* mimeData(const QModelIndexList& indices) const override
    {
        auto* mimeData = new QMimeData;

        if (indices.size() != 1 || !indices.first().isValid())
            return mimeData;

        QByteArray encodedRow;
        QDataStream stream(&encodedRow, QIODevice::WriteOnly);

        stream << _dragDropToken << qint32(indices.first().row());
        mimeData->setData(mimeTypes().first(), encodedRow);
        return mimeData;
    }

    bool canDropMimeData(const QMimeData* mimeData, Qt::DropAction action, int row, int column, const QModelIndex& parent) const override
    {
        return action == Qt::MoveAction && !parent.isValid() && column <= 0 && row >= -1 && row <= rowCount() && draggedRow(mimeData) >= 0;
    }

    bool dropMimeData(const QMimeData* mimeData, Qt::DropAction action, int row, int column, const QModelIndex& parent) override
    {
        if (!canDropMimeData(mimeData, action, row, column, parent))
            return false;

        return moveRows({}, draggedRow(mimeData), 1, {}, row < 0 ? rowCount() : row);
    }

    bool moveRows(const QModelIndex& sourceParent, int sourceRow, int count, const QModelIndex& destinationParent, int destinationRow) override
    {
        if (sourceParent.isValid() || destinationParent.isValid() || count != 1 || sourceRow < 0 || sourceRow >= rowCount() || destinationRow < 0 || destinationRow > rowCount() || destinationRow == sourceRow || destinationRow == sourceRow + 1)
            return false;

        if (!beginMoveRows({}, sourceRow, sourceRow, {}, destinationRow))
            return false;

        auto cellType = std::move(_cellTypes[sourceRow]);

        _cellTypes.erase(_cellTypes.begin() + sourceRow);
        _cellTypes.insert(_cellTypes.begin() + (destinationRow > sourceRow ? destinationRow - 1 : destinationRow), std::move(cellType));
        endMoveRows();
        return true;
    }

    const std::vector<CellTypeEntry>& getCellTypes() const { return _cellTypes; }

    void setCellTypes(std::vector<CellTypeEntry> cellTypes)
    {
        beginResetModel();
        _cellTypes     = std::move(cellTypes);
        _dragDropToken = QUuid::createUuid().toString(QUuid::WithoutBraces);
        endResetModel();
    }

private:
    int draggedRow(const QMimeData* mimeData) const
    {
        if (mimeData == nullptr || !mimeData->hasFormat(mimeTypes().first()))
            return -1;

        QDataStream stream(mimeData->data(mimeTypes().first()));
        QString dragDropToken;
        qint32 sourceRow = -1;

        stream >> dragDropToken >> sourceRow;
        return stream.status() == QDataStream::Ok && dragDropToken == _dragDropToken && sourceRow >= 0 && sourceRow < rowCount() ? sourceRow : -1;
    }

    std::vector<CellTypeEntry>    _cellTypes;
    QString                       _dragDropToken;
};

CellTypeSelectionAction::CellTypeSelectionAction(QObject* parent, const QString& title) :
    WidgetAction(parent, title),
    _cellTypeListModel(new CellTypeListModel(this)),
    _cellTypeHeadingAction(this, "Cell type heading", "Cell types"),
    _cellTypeHintAction(this, "Cell type hint", "Load a Clusters dataset to choose cell types."),
    _showAllCellTypesAction(this, "Show all"),
    _hideAllCellTypesAction(this, "Hide all")
{
    setConnectionPermissionsToForceNone(true);
    _showAllCellTypesAction.setToolTip("Show every cell type, keeping the current order.");
    _hideAllCellTypesAction.setToolTip("Hide every cell type without changing ManiVault's cluster selection.");
    _showAllCellTypesAction.setEnabled(false);
    _hideAllCellTypesAction.setEnabled(false);

    connect(_cellTypeListModel, &QAbstractItemModel::dataChanged, this, &CellTypeSelectionAction::displayChanged);
    connect(_cellTypeListModel, &QAbstractItemModel::rowsMoved, this, &CellTypeSelectionAction::displayChanged);
    connect(&_showAllCellTypesAction, &TriggerAction::triggered, this, [this] {
        setAllCellTypesVisible(true);
    });
    connect(&_hideAllCellTypesAction, &TriggerAction::triggered, this, [this] {
        setAllCellTypesVisible(false);
    });
}

QWidget* CellTypeSelectionAction::getWidget(QWidget* parent, const std::int32_t& widgetFlags)
{
    Q_UNUSED(widgetFlags)

    auto* cellTypeWidget = new QWidget(parent);
    auto* cellTypeLayout = new QVBoxLayout(cellTypeWidget);
    const auto configureLabel = [](WidgetAction*, QWidget* widget) {
        if (auto* label = qobject_cast<QLabel*>(widget)) {
            label->setTextFormat(Qt::PlainText);
            label->setWordWrap(true);
        }
    };

    cellTypeLayout->setContentsMargins(0, 0, 0, 0);
    cellTypeLayout->addWidget(_cellTypeHeadingAction.createWidget(cellTypeWidget, StringAction::Label, configureLabel));
    cellTypeLayout->addWidget(_cellTypeHintAction.createWidget(cellTypeWidget, StringAction::Label, configureLabel));

    auto* cellTypeListView = new QListView(cellTypeWidget);

    cellTypeListView->setModel(_cellTypeListModel);
    cellTypeListView->setAccessibleName("Cell types");
    cellTypeListView->setSelectionMode(QAbstractItemView::SingleSelection);
    cellTypeListView->setDragDropMode(QAbstractItemView::InternalMove);
    cellTypeListView->setDefaultDropAction(Qt::MoveAction);
    cellTypeListView->setDragDropOverwriteMode(false);
    cellTypeListView->setDropIndicatorShown(true);
    cellTypeListView->setMinimumHeight(90);
    cellTypeListView->setToolTip("Check cell types to show them. Drag rows to change the order in every gene plot.");
    cellTypeListView->setEnabled(_cellTypeListModel->rowCount() > 0);
    connect(_cellTypeListModel, &QAbstractItemModel::modelReset, cellTypeListView, [this, cellTypeListView] {
        cellTypeListView->setEnabled(_cellTypeListModel->rowCount() > 0);
    });
    cellTypeLayout->addWidget(cellTypeListView, 1);

    auto* visibilityLayout = new QHBoxLayout;

    visibilityLayout->addWidget(_showAllCellTypesAction.createWidget(cellTypeWidget, TriggerAction::Text));
    visibilityLayout->addWidget(_hideAllCellTypesAction.createWidget(cellTypeWidget, TriggerAction::Text));
    cellTypeLayout->addLayout(visibilityLayout);
    return cellTypeWidget;
}

void CellTypeSelectionAction::setCellTypes(const QString& groupingDatasetId, const std::vector<CellTypeEntry>& cellTypes)
{
    const auto previousState = groupingDatasetId == _groupingDatasetId ? getCellTypeState() : QVariantList{};

    _groupingDatasetId = groupingDatasetId;
    _cellTypeListModel->setCellTypes(cellTypes);
    restoreCellTypeState(previousState);

    if (_hasPendingCellTypeState && !groupingDatasetId.isEmpty()) {
        restoreCellTypeState(_pendingCellTypeState);
        _pendingCellTypeState.clear();
        _hasPendingCellTypeState = false;
    }

    const bool hasCellTypes = !cellTypes.empty();

    _cellTypeHintAction.setString(hasCellTypes ? "Check to show; drag to reorder." : "Load a Clusters dataset to choose cell types.");
    _showAllCellTypesAction.setEnabled(hasCellTypes);
    _hideAllCellTypesAction.setEnabled(hasCellTypes);
}

std::vector<int> CellTypeSelectionAction::getVisibleClusterIndices() const
{
    std::vector<int> clusterIndices;

    for (const auto& cellType : _cellTypeListModel->getCellTypes())
    {
        if (cellType.visible)
            clusterIndices.push_back(cellType.clusterIndex);
    }

    return clusterIndices;
}

QAbstractItemModel& CellTypeSelectionAction::getCellTypeListModel()
{
    return *_cellTypeListModel;
}

QVariantList CellTypeSelectionAction::getCellTypeState() const
{
    QVariantList cellTypeState;

    for (const auto& cellType : _cellTypeListModel->getCellTypes())
        cellTypeState.push_back(QVariantMap{{"Id", cellType.clusterId}, {"Index", cellType.clusterIndex}, {"Name", cellType.name}, {"Visible", cellType.visible}});

    return cellTypeState;
}

void CellTypeSelectionAction::restoreCellTypeState(const QVariantList& cellTypeState)
{
    const auto& cellTypes = _cellTypeListModel->getCellTypes();
    std::vector<CellTypeEntry> orderedCellTypes;
    std::vector<bool> matchedCellTypes(cellTypes.size(), false);

    orderedCellTypes.reserve(cellTypes.size());

    for (const auto& savedCellType : cellTypeState)
    {
        const auto savedState       = savedCellType.toMap();
        const auto savedClusterId   = savedState.value("Id").toString();
        const auto savedClusterName = savedState.value("Name").toString();
        const auto savedIndex       = savedState.value("Index", -1).toInt();
        int matchingRow             = -1;
        int numberOfMatches         = 0;

        for (int row = 0; row < static_cast<int>(cellTypes.size()); ++row)
        {
            const auto& cellType = cellTypes[row];
            const bool matches  = savedClusterId.isEmpty() ? cellType.clusterIndex == savedIndex && cellType.name == savedClusterName : cellType.clusterId == savedClusterId;

            if (!matchedCellTypes[row] && matches) {
                matchingRow = row;
                ++numberOfMatches;
            }
        }

        if (numberOfMatches != 1)
            continue;

        auto cellType = cellTypes[matchingRow];

        cellType.visible = savedState.value("Visible", true).toBool();
        orderedCellTypes.push_back(std::move(cellType));
        matchedCellTypes[matchingRow] = true;
    }

    // Newly added cell types appear checked after the existing ordered entries.
    for (std::size_t row = 0; row < cellTypes.size(); ++row)
    {
        if (!matchedCellTypes[row])
            orderedCellTypes.push_back(cellTypes[row]);
    }

    _cellTypeListModel->setCellTypes(std::move(orderedCellTypes));
}

void CellTypeSelectionAction::setAllCellTypesVisible(bool visible)
{
    auto cellTypes = _cellTypeListModel->getCellTypes();

    for (auto& cellType : cellTypes)
        cellType.visible = visible;

    _cellTypeListModel->setCellTypes(std::move(cellTypes));
    emit displayChanged();
}

void CellTypeSelectionAction::fromVariantMap(const QVariantMap& variantMap)
{
    WidgetAction::fromVariantMap(variantMap);
    _pendingCellTypeState    = variantMap.value("CellTypes").toList();
    _hasPendingCellTypeState = !_pendingCellTypeState.isEmpty() && _groupingDatasetId.isEmpty();

    if (!_hasPendingCellTypeState) {
        restoreCellTypeState(_pendingCellTypeState);
        _pendingCellTypeState.clear();
    }

    emit displayChanged();
}

QVariantMap CellTypeSelectionAction::toVariantMap() const
{
    auto variantMap = WidgetAction::toVariantMap();

    variantMap["CellTypes"] = _hasPendingCellTypeState ? _pendingCellTypeState : getCellTypeState();
    return variantMap;
}

}
