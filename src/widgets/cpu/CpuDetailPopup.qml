import QtQuick
import QtQuick.Window
import Theme 1.0

Window {
    id: popupWindow
    width: 320
    height: 330
    color: "transparent"
    visible: false
    flags: Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint | Qt.Tool

    property bool isPinned: false
    signal closed()

    onActiveChanged: {
        if (!active && visible && !isPinned) {
            close()
        }
    }

    Rectangle {
        id: popupCard
        anchors.fill: parent
        radius: 12
        color: "#1c1c1e"
        border.color: "#2c2c2e"
        border.width: 1
        opacity: 0
        scale: 0.95

        MouseArea {
            anchors.fill: parent
            onClicked: {}
        }

        Column {
            anchors.fill: parent
            anchors.margins: 16
            spacing: 12

            Row {
                width: parent.width

                Column {
                    spacing: 2
                    Row {
                        spacing: 4
                        Text {
                            text: "CPU"
                            color: "#ffffff"
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontLg
                            font.weight: Theme.weightLight
                        }
                        Text {
                            text: "Usage"
                            color: "#98989f"
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontLg
                            font.weight: Theme.weightExtraLight
                        }
                    }
                    Text {
                        text: "Utilization"
                        color: "#636366"
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontXs
                    }
                }

                Item {
                    width: popupCard.width - 32 - 130
                    height: 1
                }

                Rectangle {
                    width: 26
                    height: 26
                    radius: 6
                    anchors.verticalCenter: parent.verticalCenter
                    color: popupWindow.isPinned ? "#0078d4" : (pinMouse.containsMouse ? "#2c2c2e" : "transparent")

                    Text {
                        anchors.centerIn: parent
                        text: "📌"
                        font.pixelSize: Theme.fontSm
                    }

                    MouseArea {
                        id: pinMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: popupWindow.isPinned = !popupWindow.isPinned
                    }
                }
            }

            Rectangle {
                width: parent.width
                height: 100
                color: "#161618"
                radius: 6
                clip: true

                Canvas {
                    id: waveCanvas
                    anchors.fill: parent

                    Connections {
                        target: cpuAdapter
                        function onHistoryChanged() {
                            waveCanvas.requestPaint()
                        }
                    }

                    onPaint: {
                        var ctx = getContext("2d")
                        ctx.clearRect(0, 0, width, height)

                        ctx.strokeStyle = "#242428"
                        ctx.lineWidth = 1

                        for (var y = 20; y < height; y += 25) {
                            ctx.beginPath()
                            ctx.moveTo(0, y)
                            ctx.lineTo(width, y)
                            ctx.stroke()
                        }
                        for (var x = 20; x < width; x += 22) {
                            ctx.beginPath()
                            ctx.moveTo(x, 0)
                            ctx.lineTo(x, height)
                            ctx.stroke()
                        }

                        var points = cpuAdapter.history
                        if (!points || points.length < 2) return

                        var step = width / (points.length - 1)

                        ctx.beginPath()
                        ctx.moveTo(0, height - (points[0] / 100.0 * (height - 10)))

                        for (var i = 1; i < points.length; i++) {
                            var px = i * step
                            var py = height - (points[i] / 100.0 * (height - 10))
                            ctx.lineTo(px, py)
                        }

                        ctx.lineTo(width, height)
                        ctx.lineTo(0, height)
                        ctx.closePath()

                        var grad = ctx.createLinearGradient(0, 0, 0, height)
                        grad.addColorStop(0, "rgba(0, 120, 212, 0.45)")
                        grad.addColorStop(1, "rgba(0, 120, 212, 0.0)")
                        ctx.fillStyle = grad
                        ctx.fill()

                        ctx.beginPath()
                        ctx.moveTo(0, height - (points[0] / 100.0 * (height - 10)))
                        for (var j = 1; j < points.length; j++) {
                            ctx.lineTo(j * step, height - (points[j] / 100.0 * (height - 10)))
                        }
                        ctx.strokeStyle = "#0084ff"
                        ctx.lineWidth = 2
                        ctx.stroke()
                    }
                }
            }

            Grid {
                width: parent.width
                columns: 2
                spacing: 10

                Rectangle {
                    width: (parent.width - 10) / 2
                    height: 64
                    radius: 8
                    color: "#252528"

                    Column {
                        anchors.fill: parent
                        anchors.margins: 10
                        spacing: 2
                        Text {
                            text: "Usage"
                            color: "#8e8e93"
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontXs
                        }
                        Text {
                            text: cpuAdapter.cpuPercent + "%"
                            color: "#ffffff"
                            font.family: Theme.fontMono
                            font.pixelSize: Theme.fontLg
                            font.weight: Theme.weightLight
                        }
                    }
                }

                Rectangle {
                    width: (parent.width - 10) / 2
                    height: 64
                    radius: 8
                    color: "#252528"

                    Column {
                        anchors.fill: parent
                        anchors.margins: 10
                        spacing: 2
                        Text {
                            text: "Frequency"
                            color: "#8e8e93"
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontXs
                        }
                        Text {
                            text: cpuAdapter.currentFreq + " MHz"
                            color: "#ffffff"
                            font.family: Theme.fontMono
                            font.pixelSize: Theme.fontLg
                            font.weight: Theme.weightLight
                        }
                    }
                }

                Rectangle {
                    width: (parent.width - 10) / 2
                    height: 64
                    radius: 8
                    color: "#252528"

                    Column {
                        anchors.fill: parent
                        anchors.margins: 10
                        spacing: 2
                        Text {
                            text: "Cores (P / L)"
                            color: "#8e8e93"
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontXs
                        }
                        Text {
                            text: cpuAdapter.physicalCores + " / " + cpuAdapter.logicalCores
                            color: "#ffffff"
                            font.family: Theme.fontMono
                            font.pixelSize: Theme.fontLg
                            font.weight: Theme.weightLight
                        }
                    }
                }

                Rectangle {
                    width: (parent.width - 10) / 2
                    height: 64
                    radius: 8
                    color: "#252528"

                    Column {
                        anchors.fill: parent
                        anchors.margins: 10
                        spacing: 2
                        Text {
                            text: "Max frequency"
                            color: "#8e8e93"
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontXs
                        }
                        Text {
                            text: cpuAdapter.maxFreq + " MHz"
                            color: "#ffffff"
                            font.family: Theme.fontMono
                            font.pixelSize: Theme.fontLg
                            font.weight: Theme.weightLight
                        }
                    }
                }
            }
        }
    }

    ParallelAnimation {
        id: openAnim
        NumberAnimation {
            target: popupCard
            property: "opacity"
            from: 0.0
            to: 1.0
            duration: 180
            easing.type: Easing.OutQuad
        }
        NumberAnimation {
            target: popupCard
            property: "scale"
            from: 0.95
            to: 1.0
            duration: 180
            easing.type: Easing.OutCubic
        }
    }

    SequentialAnimation {
        id: closeAnim
        ParallelAnimation {
            NumberAnimation {
                target: popupCard
                property: "opacity"
                to: 0.0
                duration: 140
                easing.type: Easing.InQuad
            }
            NumberAnimation {
                target: popupCard
                property: "scale"
                to: 0.95
                duration: 140
                easing.type: Easing.InCubic
            }
        }
        ScriptAction {
            script: {
                popupWindow.visible = false
                popupWindow.closed()
            }
        }
    }

    function open(globalCenterX, globalY) {
        closeAnim.stop()

        var targetX = globalCenterX - (popupWindow.width / 2)
        var winWidth = Screen.width
        popupWindow.x = Math.max(12, Math.min(targetX, winWidth - popupWindow.width - 12))
        popupWindow.y = globalY

        popupWindow.visible = true
        popupWindow.requestActivate()
        openAnim.start()
    }

    function close() {
        openAnim.stop()
        closeAnim.start()
    }
}
