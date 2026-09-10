pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Window
import StatusBar

Item {
    id: widgetRoot

    required property CpuViewModel viewModel
    required property Window editingWindow
    required property bool editing
    property var requestEditing: null

    property bool pendingDetailOpen: false
    property bool longPressHandled: false
    readonly property CpuDetailPopup detailPopup:
        detailLoader.item as CpuDetailPopup

    function closeDetails() {
        if (detailPopup && detailPopup.visible)
            detailPopup.dismiss()
    }

    function openDetails() {
        if (!detailPopup) {
            pendingDetailOpen = true
            detailLoader.active = true
            return
        }

        const globalPoint = widgetRoot.mapToGlobal(widgetRoot.width / 2, widgetRoot.height)
        detailPopup.openAt(globalPoint.x, globalPoint.y + Theme.spacingSmall)
    }

    function toggleDetails() {
        if (editing)
            return
        if (detailPopup && detailPopup.visible)
            closeDetails()
        else
            openDetails()
    }

    onEditingChanged: {
        if (editing)
            closeDetails()
    }

    Rectangle {
        anchors.fill: parent
        radius: Math.max(Theme.spacingTiny, Math.round(widgetRoot.height * 0.15))
        color: mouseArea.containsMouse && !widgetRoot.editing
            ? Theme.hoverBackground
            : "transparent"

        Behavior on color {
            ColorAnimation {
                duration: Theme.hoverAnimationDuration
            }
        }
    }

    Row {
        anchors.centerIn: parent
        spacing: Math.max(Theme.spacingSmall, Math.round(widgetRoot.height * 0.14))

        Image {
            width: Math.max(13, Math.round(widgetRoot.height * 0.42))
            height: width
            anchors.verticalCenter: parent.verticalCenter
            source: Qt.resolvedUrl("cpu.svg")
            fillMode: Image.PreserveAspectFit
            smooth: true
            opacity: 0.9
        }

        Text {
            anchors.verticalCenter: parent.verticalCenter
            text: widgetRoot.viewModel ? widgetRoot.viewModel.cpuPercent + "%" : "0%"
            color: widgetRoot.viewModel && widgetRoot.viewModel.cpuPercent > 85
                ? Theme.error
                : Theme.foreground
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSm
            font.weight: Theme.weightLight
        }
    }

    Loader {
        id: detailLoader
        objectName: "cpuDetailLoader"
        active: false
        sourceComponent: Component {
            CpuDetailPopup {
                viewModel: widgetRoot.viewModel
                editingWindow: widgetRoot.editingWindow
            }
        }

        onLoaded: {
            if (widgetRoot.pendingDetailOpen) {
                widgetRoot.pendingDetailOpen = false
                widgetRoot.openDetails()
            }
        }
    }

    MouseArea {
        id: mouseArea
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: widgetRoot.editing ? Qt.ArrowCursor : Qt.PointingHandCursor

        onPressed: widgetRoot.longPressHandled = false
        onPressAndHold: {
            widgetRoot.longPressHandled = true
            widgetRoot.closeDetails()
            if (widgetRoot.requestEditing)
                widgetRoot.requestEditing()
        }
        onClicked: {
            if (!widgetRoot.longPressHandled)
                widgetRoot.toggleDetails()
        }
    }
}
