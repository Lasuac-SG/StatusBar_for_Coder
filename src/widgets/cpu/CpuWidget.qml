import QtQuick
import QtQuick.Window
import Theme 1.0

Item {
    id: widgetRoot
    anchors.fill: parent

    readonly property bool inEditing: Window.window ? Window.window.isEditing : false

    onInEditingChanged: {
        if (inEditing && detailPopup.visible) {
            detailPopup.close()
        }
    }

    Rectangle {
        id: hoverBg
        anchors.fill: parent
        radius: Math.max(2, Math.round(widgetRoot.height * 0.15))
        color: mouseArea.containsMouse && !widgetRoot.inEditing ? "#14ffffff" : "transparent"
        
        Behavior on color {
            ColorAnimation { duration: 150 }
        }
    }

    Row {
        anchors.centerIn: parent
        spacing: Math.max(4, Math.round(widgetRoot.height * 0.14))

        Image {
            id: cpuIcon
            width: Math.max(13, Math.round(widgetRoot.height * 0.42))
            height: width
            anchors.verticalCenter: parent.verticalCenter
            source: "qrc:/src/widgets/cpu/cpu.svg"
            sourceSize: Qt.size(width * 2, height * 2)
            fillMode: Image.PreserveAspectFit
            smooth: true
            mipmap: true
            opacity: 0.9
        }

        Text {
            anchors.verticalCenter: parent.verticalCenter
            text: cpuAdapter.cpuPercent + "%"
            color: cpuAdapter.cpuPercent > 85 ? "#ff5555" : "#e6e6e6"
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSm
            font.weight: Theme.weightLight
            renderType: Text.NativeRendering
        }
    }

    CpuDetailPopup {
        id: detailPopup
    }

    MouseArea {
        id: mouseArea
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: widgetRoot.inEditing ? Qt.ArrowCursor : Qt.PointingHandCursor

        onPressAndHold: {
            if (detailPopup.visible) {
                detailPopup.close()
            }
            if (Window.window) {
                Window.window.isEditing = true
            }
        }

        onClicked: {
            if (widgetRoot.inEditing) return

            if (detailPopup.visible) {
                detailPopup.close()
            } else {
                var globalPt = widgetRoot.mapToGlobal(widgetRoot.width / 2, 0)
                var topBarHeight = Window.window ? Window.window.height : 36
                detailPopup.open(globalPt.x, topBarHeight + 4)
            }
        }
    }
}
