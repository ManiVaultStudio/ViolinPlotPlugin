#include "GeneSelectionAction.h"

#include <QLabel>
#include <QRegularExpression>
#include <QTableView>

#include <algorithm>

using namespace mv::gui;

namespace violin {

GeneSelectionAction::GeneSelectionAction(QObject* parent, const QString& title) :
    VerticalGroupAction(parent, title),
    _geneSearchAction(this, "Search"),
    _geneDimensionsPickerAction(this, "Genes"),
    _selectionHintAction(this, "Selection hint", QString("Select genes with the checkboxes.\nUp to %1 genes per view.").arg(qulonglong(maximumDisplayedGenes)))
{
    setShowLabels(false);
    setConnectionPermissionsToForceNone(true);

    _geneSearchAction.setPlaceHolderString("Find genes / features…");
    _geneSearchAction.setClearable(true);
    _geneSearchAction.setToolTip("Filter the gene list without changing the selected genes.");
    _geneDimensionsPickerAction.setText("Genes");
    _geneDimensionsPickerAction.setToolTip(QString("Select up to %1 genes. Additional selected genes remain selected but are not plotted.").arg(qulonglong(maximumDisplayedGenes)));
    _geneDimensionsPickerAction.setStretch(1);
    _selectionHintAction.setTextElideMode(Qt::ElideNone);

    addAction(&_geneSearchAction);
    addAction(&_geneDimensionsPickerAction, -1, [this](WidgetAction*, QWidget* widget) {
        if (auto* geneTableView = widget->findChild<QTableView*>()) {
            const auto hideStatisticsColumns = [geneTableView] {
                geneTableView->setColumnHidden(1, true);
                geneTableView->setColumnHidden(2, true);
            };

            hideStatisticsColumns();
            connect(&_geneDimensionsPickerAction, &DimensionsPickerAction::proxyModelChanged, geneTableView, hideStatisticsColumns);
        }
    });
    addAction(&_selectionHintAction, StringAction::Label, [](WidgetAction*, QWidget* widget) {
        if (auto* label = qobject_cast<QLabel*>(widget)) {
            label->setTextFormat(Qt::PlainText);
            label->setWordWrap(true);
        }
    });

    connect(&_geneSearchAction, &StringAction::stringChanged, this, &GeneSelectionAction::updateNameFilter);
    connect(&_geneDimensionsPickerAction, &DimensionsPickerAction::selectedDimensionsChanged, this, [this] {
        if (_isUpdatingSelection)
            return;

        _selectedGeneDimensionIndices = _geneDimensionsPickerAction.getSelectedDimensions();
        emit selectionChanged();
    });
    connect(&_geneDimensionsPickerAction, &DimensionsPickerAction::proxyModelChanged, this, [this] {
        connect(&_geneDimensionsPickerAction.getProxyModel(), &QAbstractItemModel::modelReset, this, [this] {
            if (_isUpdatingSelection)
                return;

            const auto geneDimensionIndices = _geneDimensionsPickerAction.getSelectedDimensions();

            if (geneDimensionIndices != _selectedGeneDimensionIndices) {
                _selectedGeneDimensionIndices = geneDimensionIndices;
                emit selectionChanged();
            }
        });

        if (!_isUpdatingSelection)
            restoreSelection(true);
    });
}

void GeneSelectionAction::setPointsDataset(const mv::Dataset<Points>& pointDataset)
{
    _isUpdatingSelection = true;
    _geneDimensionsPickerAction.setPointsDataset(pointDataset);
    _isUpdatingSelection = false;

    restoreSelection(true);
}

QVector<std::int32_t> GeneSelectionAction::getSelectedDimensions() const
{
    return _geneDimensionsPickerAction.getSelectedDimensions();
}

void GeneSelectionAction::setSelectedDimensions(const QVector<std::int32_t>& geneDimensionIndices)
{
    _selectedGeneDimensionIndices = geneDimensionIndices;
    restoreSelection(false);
}

void GeneSelectionAction::restoreSelection(bool selectFirstIfEmpty)
{
    const auto numberOfDimensions = _geneDimensionsPickerAction.getEnabledDimensions().size();
    QVector<std::int32_t> geneDimensionIndices;

    for (const auto geneDimensionIndex : _selectedGeneDimensionIndices)
    {
        if (geneDimensionIndex >= 0 && geneDimensionIndex < numberOfDimensions)
            geneDimensionIndices.push_back(geneDimensionIndex);
    }

    std::sort(geneDimensionIndices.begin(), geneDimensionIndices.end());
    geneDimensionIndices.erase(std::unique(geneDimensionIndices.begin(), geneDimensionIndices.end()), geneDimensionIndices.end());

    if (geneDimensionIndices.isEmpty() && selectFirstIfEmpty && numberOfDimensions > 0)
        geneDimensionIndices.push_back(0);

    _isUpdatingSelection = true;
    _geneDimensionsPickerAction.selectDimensions(geneDimensionIndices);
    _selectedGeneDimensionIndices = geneDimensionIndices;
    updateNameFilter();
    _isUpdatingSelection = false;

    emit selectionChanged();
}

void GeneSelectionAction::updateNameFilter()
{
    _geneDimensionsPickerAction.getProxyModel().setFilterCaseSensitivity(Qt::CaseInsensitive);
    _geneDimensionsPickerAction.setNameFilter(QRegularExpression::escape(_geneSearchAction.getString()));
}

void GeneSelectionAction::fromVariantMap(const QVariantMap& variantMap)
{
    GroupAction::fromVariantMap(variantMap);

    _isUpdatingSelection = true;
    _geneDimensionsPickerAction.fromParentVariantMap(variantMap);
    _geneSearchAction.fromParentVariantMap(variantMap);
    _selectedGeneDimensionIndices = _geneDimensionsPickerAction.getSelectedDimensions();
    updateNameFilter();
    _isUpdatingSelection = false;

    emit selectionChanged();
}

QVariantMap GeneSelectionAction::toVariantMap() const
{
    auto variantMap = GroupAction::toVariantMap();

    _geneDimensionsPickerAction.insertIntoVariantMap(variantMap);
    _geneSearchAction.insertIntoVariantMap(variantMap);

    return variantMap;
}

void GeneSelectionAction::fromLegacyVariantMap(const QVariantMap& variantMap)
{
    if (variantMap.contains("Dimensions")) {
        QVector<std::int32_t> geneDimensionIndices;

        for (const auto& value : variantMap.value("Dimensions").toList())
            geneDimensionIndices.push_back(value.toInt());

        setSelectedDimensions(geneDimensionIndices);
    }

    _geneSearchAction.setString(variantMap.value("Search").toString());
}

}
