import QtQuick
import QtQuick.Window

Window {
    id: root
    x: 0
    y: 0
    width: Screen.width
    
    readonly property real spaceHeight: (typeof reservedBarHeight !== "undefined" && reservedBarHeight > 0) ? reservedBarHeight : 36
    readonly property real barHeight: Math.round(spaceHeight * 0.9)
    readonly property real verticalMargin: Math.max(0, (spaceHeight - barHeight) / 2)

    readonly property real cellWidth: Math.round(barHeight * 1.0)
    readonly property real spacing: Math.max(4, Math.round(barHeight * 0.2))
    readonly property real unitWidth: cellWidth + spacing
    readonly property real barRadius: Math.round(barHeight * 0.25)

    // 高度严格恒定为 spaceHeight，绝不在底层生成透明拦截层
    height: spaceHeight
    visible: true
    color: "transparent"
    
    flags: Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint | Qt.Tool

    property bool isEditing: false

    Connections {
        target: widgetModel
        function onPersistenceError(message) {
            console.error("[WidgetModel] " + message)
        }
    }

    Rectangle {
        id: container
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.leftMargin: Math.round(root.barHeight * 0.3)
        anchors.rightMargin: Math.round(root.barHeight * 0.3)
        anchors.topMargin: root.verticalMargin
        height: root.barHeight

        color: "#1e1e1e"
        radius: root.barRadius
        border.color: root.isEditing ? "#8be9fd" : "transparent"
        border.width: root.isEditing ? 1.5 : 0
        clip: false

        Item {
            id: gridArea
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            anchors.topMargin: 2
            anchors.bottomMargin: 2
            
            property real availableWidth: Math.max(0, container.width - (root.spacing * 2))
            property int rawSlots: Math.floor((availableWidth + root.spacing) / root.unitWidth)
            property int totalSlots: (rawSlots % 2 === 1) ? rawSlots : Math.max(1, rawSlots - 1)
            
            width: (totalSlots * root.cellWidth) + (Math.max(0, totalSlots - 1) * root.spacing)
            anchors.horizontalCenter: parent.horizontalCenter

            Repeater {
                model: widgetModel
                delegate: Rectangle {
                    id: widgetContainer
                    
                    x: model.slot * root.unitWidth
                    width: (model.span * root.cellWidth) + (Math.max(0, model.span - 1) * root.spacing)
                    height: parent.height
                    
                    color: "transparent"
                    radius: Math.round(root.barRadius * 0.6)

                    Behavior on x {
                        NumberAnimation { duration: 250; easing.type: Easing.InOutQuad }
                    }
                    Behavior on color {
                        ColorAnimation { duration: 250 }
                    }

                    Loader {
                        anchors.fill: parent
                        source: {
                            if (model.kind === "Clock") return "qrc:/src/widgets/clock/ClockWidget.qml"
                            if (model.kind === "Cpu") return "qrc:/src/widgets/cpu/CpuWidget.qml"
                            return ""
                        }
                    }

                    MouseArea {
                        anchors.fill: parent
                        drag.target: root.isEditing ? widgetContainer : null
                        drag.axis: Drag.XAxis
                        enabled: root.isEditing
                        z: root.isEditing ? 10 : 0
                        
                        onPressed: {
                            widgetContainer.color = "#1affffff"
                        }

                        onReleased: {
                            widgetContainer.color = "transparent"
                            let dropCenterX = widgetContainer.x + (widgetContainer.width / 2)
                            widgetModel.handleWidgetDropped(model.index, dropCenterX, root.cellWidth, root.spacing, gridArea.width)
                            
                            widgetContainer.x = Qt.binding(function() {
                                return model.slot * root.unitWidth
                            })
                        }
                    }
                }
            }
        }

        MouseArea {
            anchors.fill: parent
            z: -1
            onPressAndHold: {
                root.isEditing = !root.isEditing
            }
            onClicked: {
                if (root.isEditing) root.isEditing = false
            }
        }
    }
}
