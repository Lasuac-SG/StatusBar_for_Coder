import QtQuick
import QtQuick.Window
import StatusBar

Item {
    id: host

    required property url qmlUrl
    required property WidgetViewModel viewModel
    required property Window editingWindow
    required property bool editing
    signal editingRequested()

    property bool componentComplete: false
    property bool updateScheduled: false
    property url loadedQmlUrl
    property WidgetViewModel loadedViewModel: null
    property Window loadedEditingWindow: null

    function scheduleUpdate() {
        if (!componentComplete || updateScheduled)
            return
        updateScheduled = true
        Qt.callLater(function() {
            updateScheduled = false
            updateSource()
        })
    }

    function updateSource() {
        if (!componentComplete)
            return

        if (qmlUrl.toString().length === 0 || viewModel === null) {
            if (loader.source.toString().length !== 0)
                loader.setSource("")
            loadedQmlUrl = qmlUrl
            loadedViewModel = viewModel
            loadedEditingWindow = editingWindow
            return
        }

        if (loadedQmlUrl === qmlUrl
                && loadedViewModel === viewModel
                && loadedEditingWindow === editingWindow) {
            return
        }

        loadedQmlUrl = qmlUrl
        loadedViewModel = viewModel
        loadedEditingWindow = editingWindow
        loader.setSource(qmlUrl, {
            viewModel: viewModel,
            editingWindow: editingWindow,
            editing: Qt.binding(function() {
                return host.editing
            }),
            requestEditing: function() {
                host.editingRequested()
            }
        })
    }

    onQmlUrlChanged: scheduleUpdate()
    onViewModelChanged: scheduleUpdate()
    onEditingWindowChanged: scheduleUpdate()
    Component.onCompleted: {
        componentComplete = true
        scheduleUpdate()
    }

    Loader {
        id: loader
        objectName: "widgetLoader"
        anchors.fill: parent

        onStatusChanged: {
            if (status === Loader.Error)
                console.error("Failed to load widget " + host.qmlUrl)
        }
    }
}
