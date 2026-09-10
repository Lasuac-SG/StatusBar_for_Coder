pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Window
import StatusBar

Window {
    id: root
    objectName: "statusBarRoot"

    required property WidgetModel widgetModel
    property bool isEditing: false

    readonly property real barHeight: Math.max(1, Math.round(height * 0.9))
    readonly property real verticalMargin: Math.max(0, (height - barHeight) / 2)
    readonly property real cellWidth: barHeight
    readonly property real spacing: Math.max(Theme.spacingSmall, Math.round(barHeight * 0.2))
    readonly property real unitWidth: cellWidth + spacing
    readonly property real barRadius: Math.round(barHeight * Theme.barRadiusRatio)

    x: 0
    y: 0
    width: Screen.width
    height: Theme.baseBarHeight
    visible: false
    color: "transparent"
    flags: Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint | Qt.Tool

    Connections {
        target: root.widgetModel

        function onPersistenceError(message) {
            console.error("[WidgetModel] " + message)
        }
    }

    Rectangle {
        id: container
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.leftMargin: Math.round(root.barHeight * Theme.outerMarginRatio)
        anchors.rightMargin: anchors.leftMargin
        anchors.topMargin: root.verticalMargin
        height: root.barHeight
        color: Theme.barBackground
        radius: root.barRadius
        border.color: root.isEditing ? Theme.editingBorder : "transparent"
        border.width: root.isEditing ? Theme.editingBorderWidth : 0

        TapHandler {
            onLongPressed: root.isEditing = !root.isEditing
        }

        Item {
            id: gridArea
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            anchors.topMargin: Theme.spacingTiny
            anchors.bottomMargin: Theme.spacingTiny
            anchors.horizontalCenter: parent.horizontalCenter

            readonly property real availableWidth: Math.max(
                0, container.width - (root.spacing * 2))
            readonly property int rawSlots: root.unitWidth > 0
                ? Math.max(0, Math.floor((availableWidth + root.spacing) / root.unitWidth))
                : 0
            readonly property int totalSlots: rawSlots <= 0
                ? 0
                : (rawSlots % 2 === 1 ? rawSlots : rawSlots - 1)

            width: (totalSlots * root.cellWidth)
                + (Math.max(0, totalSlots - 1) * root.spacing)

            function syncTotalSlots() {
                root.widgetModel.setTotalSlots(totalSlots)
            }

            onTotalSlotsChanged: syncTotalSlots()
            Component.onCompleted: syncTotalSlots()

            Repeater {
                model: root.widgetModel

                delegate: Rectangle {
                    id: widgetContainer

                    required property string instanceId
                    required property string type
                    required property int slot
                    required property int span
                    required property url qmlUrl
                    required property WidgetViewModel viewModel

                    x: slot * root.unitWidth
                    width: (span * root.cellWidth)
                        + (Math.max(0, span - 1) * root.spacing)
                    height: gridArea.height
                    color: "transparent"
                    radius: Math.round(root.barRadius * Theme.widgetRadiusRatio)

                    function restorePositionBinding() {
                        x = Qt.binding(function() {
                            return widgetContainer.slot * root.unitWidth
                        })
                    }

                    Behavior on x {
                        enabled: !dragArea.drag.active
                        NumberAnimation {
                            duration: Theme.layoutAnimationDuration
                            easing.type: Easing.InOutQuad
                        }
                    }

                    Behavior on color {
                        ColorAnimation {
                            duration: Theme.hoverAnimationDuration
                        }
                    }

                    WidgetHost {
                        anchors.fill: parent
                        qmlUrl: widgetContainer.qmlUrl
                        viewModel: widgetContainer.viewModel
                        editingWindow: root
                        editing: root.isEditing
                        onEditingRequested: root.isEditing = true
                    }

                    MouseArea {
                        id: dragArea
                        anchors.fill: parent
                        enabled: root.isEditing
                        z: enabled ? 10 : 0
                        drag.target: enabled ? widgetContainer : null
                        drag.axis: Drag.XAxis
                        drag.minimumX: 0
                        drag.maximumX: Math.max(0, gridArea.width - widgetContainer.width)

                        onPressed: widgetContainer.color = Theme.dragBackground
                        onCanceled: {
                            widgetContainer.color = "transparent"
                            widgetContainer.restorePositionBinding()
                        }
                        onReleased: {
                            widgetContainer.color = "transparent"
                            const targetSlot = root.unitWidth > 0
                                ? Math.round(widgetContainer.x / root.unitWidth)
                                : widgetContainer.slot
                            root.widgetModel.dropWidget(widgetContainer.instanceId, targetSlot)
                            widgetContainer.restorePositionBinding()
                        }
                    }
                }
            }
        }

        MouseArea {
            anchors.fill: parent
            z: -1
            enabled: root.isEditing
            onClicked: root.isEditing = false
        }
    }
}
