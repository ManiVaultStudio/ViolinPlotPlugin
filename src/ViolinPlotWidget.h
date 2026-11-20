#pragma once

#include "ViolinStatistics.h"

#include <widgets/WebWidget.h>

#include <QColor>
#include <QHash>
#include <QList>
#include <QSet>
#include <vector>

class ViolinPlotWidget;

// Same named QtBridge object and js_available handshake as HeatMap.
class ViolinPlotCommunicationObject : public mv::gui::WebCommunicationObject
{
    Q_OBJECT

public:
    explicit ViolinPlotCommunicationObject(ViolinPlotWidget* parent);

signals:
    void qt_setData(const QString& json);
    void qt_setOptions(const QString& json);
    void qt_setSelection(const QList<int>& selectedClusterIndices);
    void qt_setMessage(const QString& message);
    void qt_exportPlot(int requestId, const QString& format);

public slots:
    void js_groupActivated(int clusterIndex, bool toggle);
    void js_exportReady(int requestId, const QString& payload, const QString& error);
    void js_renderError(const QString& error);

private:
    ViolinPlotWidget*                 _violinPlotWidget;
};

struct ViolinGroup
{
    QString name;
    QColor color;
    int     clusterIndex = -1;
};

struct ViolinPanel
{
    QString name;
    std::vector<violin::Distribution> distributions;
};

class ViolinPlotWidget : public mv::gui::WebWidget
{
    Q_OBJECT

public:
    explicit ViolinPlotWidget(QWidget* parent = nullptr);
    void setData(std::vector<ViolinGroup> groups, std::vector<ViolinPanel> panels);
    void setDisplayOptions(bool sharedAxis, bool showDots, bool log1p, bool includeZeros = true);
    void setSelectedClusters(const QSet<int>& selectedClusterIndices);
    void setMessage(const QString& message);
    void exportPlot(const QString& fileName, bool exportAsSvg);

signals:
    void groupActivated(int clusterIndex, Qt::KeyboardModifiers modifiers);
    void exportFinished(bool success, const QString& error);
    void renderFailed(const QString& error);

private slots:
    void initWebPage() override;

private:
    friend class ViolinPlotCommunicationObject;

    void receiveExport(int requestId, const QString& payload, const QString& error);

    struct ExportRequest
    {
        QString fileName;
        bool    exportAsSvg;
    };

    ViolinPlotCommunicationObject*    _communicationObject;
    QString                           _plotDataJson = "{\"groups\":[],\"panels\":[]}";
    QString                           _displayOptionsJson = "{}";
    QString                           _statusMessage = "Drop an expression Points dataset here.";
    QList<int>                        _selectedClusterIndices;
    QHash<int, ExportRequest>         _pendingExportRequests;
    int                               _nextExportRequestId = 0;
    bool                              _isWebPageReady = false;
};
