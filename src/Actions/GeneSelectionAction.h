#pragma once

#include <PointData/DimensionsPickerAction.h>

#include <actions/StringAction.h>
#include <actions/VerticalGroupAction.h>

#include <cstddef>

namespace violin {

/** Gene search and selection using ManiVault's dimension picker. */
class GeneSelectionAction : public mv::gui::VerticalGroupAction
{
    Q_OBJECT

public:
    static constexpr std::size_t maximumDisplayedGenes = 10;

    GeneSelectionAction(QObject* parent, const QString& title);

    void setPointsDataset(const mv::Dataset<Points>& pointDataset);
    QVector<std::int32_t> getSelectedDimensions() const;
    void setSelectedDimensions(const QVector<std::int32_t>& geneDimensionIndices);

public: // Action getters
    DimensionsPickerAction& getGeneDimensionsPickerAction() { return _geneDimensionsPickerAction; }
    mv::gui::StringAction& getGeneSearchAction() { return _geneSearchAction; }

public: // Serialization
    void fromVariantMap(const QVariantMap& variantMap) override;
    QVariantMap toVariantMap() const override;

signals:
    void selectionChanged();

private:
    void restoreSelection(bool selectFirstIfEmpty);
    void updateNameFilter();

    mv::gui::StringAction     _geneSearchAction;
    DimensionsPickerAction    _geneDimensionsPickerAction;
    mv::gui::StringAction     _selectionHintAction;
    QVector<std::int32_t>     _selectedGeneDimensionIndices;
    bool                      _isUpdatingSelection = false;
};

}
