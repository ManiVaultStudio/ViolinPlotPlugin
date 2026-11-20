/* QtBridge wiring follows HeatMap's QWebChannel/js_available handshake.
 * Loading this page outside ManiVault also works for automated browser tests.
 */
(() => {
    "use strict";
    const transport = window.qt?.webChannelTransport;
    if (!transport) return;
    const connect = () => {
        new QWebChannel(transport, channel => {
            const bridge = channel.objects.QtBridge;
            const chart = window.ViolinChart;
            if (!chart) { bridge.js_renderError("The bundled D3 chart could not be initialized."); return; }
            const safe = callback => value => {
                try { callback(value); } catch (error) { bridge.js_renderError(String(error.message || error)); }
            };
            bridge.qt_setData.connect(safe(chart.setData));
            bridge.qt_setOptions.connect(safe(chart.setOptions));
            bridge.qt_setSelection.connect(safe(chart.setSelection));
            bridge.qt_setMessage.connect(safe(chart.setMessage));
            bridge.qt_exportPlot.connect(async (id, format) => {
                try { bridge.js_exportReady(id, await chart.exportPlot(format), ""); }
                catch (error) { bridge.js_exportReady(id, "", String(error.message || error)); }
            });
            chart.onGroupActivated((index, toggle) => bridge.js_groupActivated(index, toggle));
            chart.onError(error => bridge.js_renderError(error));
            bridge.js_available(); // Connect every signal before C++ flushes buffered state.
        });
    };
    if (typeof window.QWebChannel === "function") connect();
    else {
        const script = document.createElement("script");
        script.src = "qrc:///qtwebchannel/qwebchannel.js";
        script.onload = connect;
        script.onerror = () => { document.getElementById("empty-message").textContent = "Qt WebChannel could not be loaded."; };
        document.head.appendChild(script);
    }
})();
