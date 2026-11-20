#include "ViolinPlotWidget.h"

#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLayout>
#include <QSaveFile>
#include <QTimer>
#include <QWebEnginePage>

namespace
{
QString serializeJsonObject(const QJsonObject& value)
{
    return QString::fromUtf8(QJsonDocument(value).toJson(QJsonDocument::Compact));
}

QJsonObject serializeDistribution(const violin::Distribution& distribution)
{
    QJsonArray density;
    QJsonArray dots;

    for (const auto& point : distribution.density)
        density.append(QJsonArray{point.value, point.density});

    for (const auto value : distribution.dots)
        dots.append(value);

    return {
        {"total", double(distribution.total)}, {"finite", double(distribution.finite)}, {"count", double(distribution.count)},
        {"detected", double(distribution.detected)}, {"invalid", double(distribution.invalid)},
        {"negativeLogValues", double(distribution.negativeLogValues)},
        {"minimum", distribution.minimum}, {"maximum", distribution.maximum}, {"mean", distribution.mean},
        {"q1", distribution.q1}, {"median", distribution.median}, {"q3", distribution.q3}, {"bandwidth", distribution.bandwidth},
        {"constant", distribution.constant}, {"density", density}, {"dots", dots}
    };
}
}

ViolinPlotCommunicationObject::ViolinPlotCommunicationObject(ViolinPlotWidget* parent) :
    _violinPlotWidget(parent)
{
    setParent(parent);
}

void ViolinPlotCommunicationObject::js_groupActivated(int clusterIndex, bool toggle)
{
    emit _violinPlotWidget->groupActivated(clusterIndex, toggle ? Qt::ControlModifier : Qt::NoModifier);
}

void ViolinPlotCommunicationObject::js_exportReady(int requestId, const QString& payload, const QString& error)
{
    _violinPlotWidget->receiveExport(requestId, payload, error);
}

void ViolinPlotCommunicationObject::js_renderError(const QString& error)
{
    emit _violinPlotWidget->renderFailed(error);
}

ViolinPlotWidget::ViolinPlotWidget(QWidget* parent) :
    _communicationObject(new ViolinPlotCommunicationObject(this))
{
    setParent(parent);
    Q_INIT_RESOURCE(violin_resources);
    init(_communicationObject);
    setAcceptDrops(true);

    if (layout() != nullptr)
        layout()->setContentsMargins(0, 0, 0, 0);

    connect(getPage(), &QWebEnginePage::loadStarted, this, [this] {
        _isWebPageReady = false;
    });
    connect(getPage(), &QWebEnginePage::loadFinished, this, [this](bool success) {
        if (!success)
            emit renderFailed("The embedded violin chart could not be loaded.");
    });

    setPage(":/violin/violin.html", "qrc:/violin/");
}

void ViolinPlotWidget::initWebPage()
{
    // Re-send the latest state on first load and on every page reload.
    _isWebPageReady = true;
    emit _communicationObject->qt_setOptions(_displayOptionsJson);
    emit _communicationObject->qt_setData(_plotDataJson);
    emit _communicationObject->qt_setSelection(_selectedClusterIndices);
    emit _communicationObject->qt_setMessage(_statusMessage);
}

void ViolinPlotWidget::setData(std::vector<ViolinGroup> groups, std::vector<ViolinPanel> panels)
{
    QJsonArray groupArray;
    QJsonArray panelArray;

    for (const auto& group : groups)
    {
        const QColor color = group.color.isValid() ? group.color : QColor("#7299cf");

        groupArray.append(QJsonObject{{"name", group.name}, {"color", color.name()}, {"clusterIndex", group.clusterIndex}});
    }

    for (const auto& panel : panels)
    {
        QJsonArray distributions;

        for (const auto& distribution : panel.distributions)
            distributions.append(serializeDistribution(distribution));

        panelArray.append(QJsonObject{{"name", panel.name}, {"distributions", distributions}});
    }

    _plotDataJson = serializeJsonObject({{"groups", groupArray}, {"panels", panelArray}});

    if (_isWebPageReady)
        emit _communicationObject->qt_setData(_plotDataJson);
}

void ViolinPlotWidget::setDisplayOptions(bool sharedAxis, bool showDots, bool log1p, bool includeZeros)
{
    _displayOptionsJson = serializeJsonObject({{"sharedAxis", sharedAxis}, {"showDots", showDots}, {"log1p", log1p}, {"includeZeros", includeZeros}});

    if (_isWebPageReady)
        emit _communicationObject->qt_setOptions(_displayOptionsJson);
}

void ViolinPlotWidget::setSelectedClusters(const QSet<int>& selectedClusterIndices)
{
    _selectedClusterIndices = selectedClusterIndices.values();

    if (_isWebPageReady)
        emit _communicationObject->qt_setSelection(_selectedClusterIndices);
}

void ViolinPlotWidget::setMessage(const QString& message)
{
    _statusMessage = message;

    if (_isWebPageReady)
        emit _communicationObject->qt_setMessage(_statusMessage);
}

void ViolinPlotWidget::exportPlot(const QString& fileName, bool exportAsSvg)
{
    if (!_isWebPageReady) {
        emit exportFinished(false, "The chart is still loading. Try exporting again shortly.");
        return;
    }

    const int requestId = ++_nextExportRequestId;

    _pendingExportRequests.insert(requestId, {fileName, exportAsSvg});
    emit _communicationObject->qt_exportPlot(requestId, exportAsSvg ? "svg" : "png");
    QTimer::singleShot(20000, this, [this, requestId] {
        if (_pendingExportRequests.remove(requestId))
            emit exportFinished(false, "The chart did not finish exporting. Try SVG for a large plot.");
    });
}

void ViolinPlotWidget::receiveExport(int requestId, const QString& payload, const QString& error)
{
    const auto exportRequestIterator = _pendingExportRequests.find(requestId);

    // Ignore stale responses after timeout.
    if (exportRequestIterator == _pendingExportRequests.end())
        return;

    const auto exportRequest = exportRequestIterator.value();

    _pendingExportRequests.erase(exportRequestIterator);

    if (!error.isEmpty()) {
        emit exportFinished(false, error);
        return;
    }

    QByteArray imageData;

    if (exportRequest.exportAsSvg) {
        if (!payload.contains("<svg")) {
            emit exportFinished(false, "The chart returned an invalid SVG.");
            return;
        }

        imageData = payload.toUtf8();
    } else {
        const QString pngDataUrlPrefix = "data:image/png;base64,";

        if (!payload.startsWith(pngDataUrlPrefix)) {
            emit exportFinished(false, "The chart returned an invalid PNG.");
            return;
        }

        imageData = QByteArray::fromBase64(payload.mid(pngDataUrlPrefix.size()).toLatin1());

        if (QImage::fromData(imageData, "PNG").isNull()) {
            emit exportFinished(false, "The PNG could not be decoded.");
            return;
        }
    }

    QSaveFile exportFile(exportRequest.fileName);

    if (!exportFile.open(QIODevice::WriteOnly) || exportFile.write(imageData) != imageData.size() || !exportFile.commit()) {
        emit exportFinished(false, "The plot could not be written: " + exportFile.errorString());
        return;
    }

    emit exportFinished(true, {});
}
