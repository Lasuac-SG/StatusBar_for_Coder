import QtQuick
import QtQuick.Window

Window {
    id: root
    x: 0
    y: 0
    width: Screen.width
    height: 40
    visible: true
    color: "transparent"
    
    flags: Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint | Qt.Tool

    property real dpiScale: Screen.devicePixelRatio
    property bool isEditing: false

    // 基础单元格尺寸配置
    property real cellWidth: 40 * root.dpiScale
    property real spacing: 8 * root.dpiScale
    property real unitWidth: cellWidth + spacing

    Rectangle {
        id: container
        anchors.fill: parent
        anchors.leftMargin: 12 * root.dpiScale
        anchors.rightMargin: 12 * root.dpiScale
        anchors.topMargin: 4 * root.dpiScale
        anchors.bottomMargin: 4 * root.dpiScale

        color: "#1e1e1e"
        radius: 10 * root.dpiScale
        border.color: root.isEditing ? "#8be9fd" : "transparent"
        border.width: root.isEditing ? 2 * root.dpiScale : 0
        clip: true

        // 核心：强制奇数槽位 + 水平居中网格区
        Item {
            id: gridArea
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            anchors.topMargin: 4 * root.dpiScale
            anchors.bottomMargin: 4 * root.dpiScale
            
            // 1. 计算可用空间内最多容纳多少个槽位
            property real availableWidth: parent.width - 16 * root.dpiScale
            property int rawSlots: Math.floor((availableWidth + root.spacing) / root.unitWidth)
            
            // 2. 强制收敛为奇数
            property int totalSlots: (rawSlots % 2 === 1) ? rawSlots : Math.max(1, rawSlots - 1)
            
            // 3. 计算奇数网格总宽度并水平居中
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
                    radius: 6 * root.dpiScale

                    Behavior on x {
                        NumberAnimation { duration: 250; easing.type: Easing.InOutQuad }
                    }
                    Behavior on color {
                        ColorAnimation { duration: 250 }
                    }

                    Loader {
                        anchors.fill: parent
                        sourceComponent: model.kind === "Clock" ? clockComponent : null
                    }

                    MouseArea {
                        anchors.fill: parent
                        drag.target: root.isEditing ? widgetContainer : null
                        drag.axis: Drag.XAxis
                        
                        onPressAndHold: {
                            root.isEditing = true
                        }
                        
                        onPressed: {
                            if (root.isEditing) {
                                widgetContainer.color = "#1affffff"
                            }
                        }

                        onReleased: {
                            if (root.isEditing) {
                                widgetContainer.color = "transparent"
                                let dropCenterX = widgetContainer.x + (widgetContainer.width / 2)
                                
                                // 传入当前奇数网格区的实际宽度供 C++ 计算最大限制
                                widgetModel.handleWidgetDropped(model.index, dropCenterX, root.dpiScale, gridArea.width)
                                
                                widgetContainer.x = Qt.binding(function() {
                                    return model.slot * root.unitWidth
                                })
                            }
                        }
                    }
                }
            }
        }
    }

    Component {
        id: clockComponent
        ClockWidget {}
    }

    MouseArea {
        anchors.fill: parent
        z: -1
        onClicked: {
            if (root.isEditing) root.isEditing = false
        }
    }
}
