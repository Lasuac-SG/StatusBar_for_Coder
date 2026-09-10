import QtQuick
import QtQuick.Window
import StatusBar

Window {
    id: popupWindow
    objectName: "cpuDetailPopup"

    required property CpuViewModel viewModel
    required property Window editingWindow
    property bool isPinned: false

    width: 320
    height: 330
    visible: false
    color: "transparent"
    flags: Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint | Qt.Tool
    transientParent: editingWindow

    onActiveChanged: {
        if (!active && visible && !isPinned)
            dismiss()
    }

    function openAt(globalCenterX, globalY) {
        closeAnimation.stop()
        const targetX = globalCenterX - (width / 2)
        x = Math.max(
            Theme.spacingLarge,
            Math.min(targetX, Screen.width - width - Theme.spacingLarge))
        y = globalY
        popupCard.opacity = 0
        popupCard.scale = 0.95
        visible = true
        requestActivate()
        waveCanvas.requestPaint()
        openAnimation.start()
    }

    function dismiss() {
        if (!visible || closeAnimation.running)
            return
        openAnimation.stop()
        closeAnimation.start()
    }

    Rectangle {
        id: popupCard
        anchors.fill: parent
        radius: Theme.radiusPopup
        color: Theme.popupBackground
        border.color: Theme.border
        border.width: 1
        opacity: 0
        scale: 0.95

        Column {
            anchors.fill: parent
            anchors.margins: Theme.spacingExtraLarge
            spacing: Theme.spacingLarge

            Row {
                width: parent.width

                Column {
                    id: titleColumn
                    spacing: Theme.spacingTiny

                    Row {
                        spacing: Theme.spacingSmall

                        Text {
                            text: "CPU"
                            color: Theme.foreground
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontLg
                            font.weight: Theme.weightLight
                        }

                        Text {
                            text: "Usage"
                            color: Theme.foregroundMuted
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontLg
                            font.weight: Theme.weightExtraLight
                        }
                    }

                    Text {
                        text: "Utilization"
                        color: Theme.foregroundSubtle
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontXs
                    }
                }

                Item {
                    width: Math.max(
                        0, parent.width - titleColumn.width - pinButton.width)
                    height: 1
                }

                Rectangle {
                    id: pinButton
                    width: 26
                    height: 26
                    radius: Theme.radiusControl
                    anchors.verticalCenter: parent.verticalCenter
                    color: popupWindow.isPinned
                        ? Theme.accentStrong
                        : (pinMouse.containsMouse ? Theme.border : "transparent")

                    Text {
                        anchors.centerIn: parent
                        text: "P"
                        color: Theme.foreground
                        font.family: Theme.fontFamily
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
                radius: Theme.radiusControl
                color: Theme.chartBackground
                clip: true

                Canvas {
                    id: gridCanvas
                    anchors.fill: parent

                    onWidthChanged: requestPaint()
                    onHeightChanged: requestPaint()
                    onPaint: {
                        const context = getContext("2d")
                        context.clearRect(0, 0, width, height)
                        context.strokeStyle = Theme.chartGrid
                        context.lineWidth = 1

                        for (let lineY = 20; lineY < height; lineY += 25) {
                            context.beginPath()
                            context.moveTo(0, lineY)
                            context.lineTo(width, lineY)
                            context.stroke()
                        }
                        for (let lineX = 20; lineX < width; lineX += 22) {
                            context.beginPath()
                            context.moveTo(lineX, 0)
                            context.lineTo(lineX, height)
                            context.stroke()
                        }
                    }
                }

                Canvas {
                    id: waveCanvas
                    anchors.fill: parent

                    onWidthChanged: {
                        if (popupWindow.visible)
                            requestPaint()
                    }
                    onHeightChanged: {
                        if (popupWindow.visible)
                            requestPaint()
                    }
                    onPaint: {
                        const context = getContext("2d")
                        context.clearRect(0, 0, width, height)
                        if (!popupWindow.visible || width <= 0 || height <= 0)
                            return

                        const points = popupWindow.viewModel
                            ? popupWindow.viewModel.history
                            : []
                        if (!points || points.length < 2)
                            return

                        const step = width / Math.max(1, points.length - 1)
                        const graphHeight = Math.max(0, height - Theme.spacingMedium)
                        function pointY(value) {
                            const numeric = Number(value)
                            const safeValue = isFinite(numeric)
                                ? Math.max(0, Math.min(100, numeric))
                                : 0
                            return height - (safeValue / 100 * graphHeight)
                        }

                        context.beginPath()
                        context.moveTo(0, pointY(points[0]))
                        for (let index = 1; index < points.length; ++index)
                            context.lineTo(index * step, pointY(points[index]))
                        context.lineTo(width, height)
                        context.lineTo(0, height)
                        context.closePath()
                        context.fillStyle = Theme.chartFill
                        context.fill()

                        context.beginPath()
                        context.moveTo(0, pointY(points[0]))
                        for (let index = 1; index < points.length; ++index)
                            context.lineTo(index * step, pointY(points[index]))
                        context.strokeStyle = Theme.accent
                        context.lineWidth = 2
                        context.stroke()
                    }

                    Connections {
                        target: popupWindow.viewModel
                        enabled: popupWindow.visible

                        function onHistoryChanged() {
                            waveCanvas.requestPaint()
                        }
                    }
                }
            }

            Grid {
                width: parent.width
                columns: 2
                spacing: Theme.spacingMedium

                MetricCard {
                    width: (parent.width - parent.spacing) / 2
                    height: Theme.metricCardHeight
                    label: "Usage"
                    value: popupWindow.viewModel
                        ? popupWindow.viewModel.cpuPercent.toString()
                        : "0"
                    unit: "%"
                    accent: popupWindow.viewModel && popupWindow.viewModel.cpuPercent > 85
                        ? Theme.error
                        : Theme.foreground
                }

                MetricCard {
                    width: (parent.width - parent.spacing) / 2
                    height: Theme.metricCardHeight
                    label: "Frequency"
                    value: popupWindow.viewModel
                        ? popupWindow.viewModel.currentFrequencyMHz.toString()
                        : "0"
                    unit: "MHz"
                }

                MetricCard {
                    width: (parent.width - parent.spacing) / 2
                    height: Theme.metricCardHeight
                    label: "Cores (P / L)"
                    value: popupWindow.viewModel
                        ? popupWindow.viewModel.physicalCores
                            + " / " + popupWindow.viewModel.logicalCores
                        : "0 / 0"
                }

                MetricCard {
                    width: (parent.width - parent.spacing) / 2
                    height: Theme.metricCardHeight
                    label: "Max frequency"
                    value: popupWindow.viewModel
                        ? popupWindow.viewModel.maxFrequencyMHz.toString()
                        : "0"
                    unit: "MHz"
                }
            }
        }
    }

    ParallelAnimation {
        id: openAnimation

        NumberAnimation {
            target: popupCard
            property: "opacity"
            from: 0
            to: 1
            duration: Theme.popupOpenDuration
            easing.type: Easing.OutQuad
        }
        NumberAnimation {
            target: popupCard
            property: "scale"
            from: 0.95
            to: 1
            duration: Theme.popupOpenDuration
            easing.type: Easing.OutCubic
        }
    }

    SequentialAnimation {
        id: closeAnimation

        ParallelAnimation {
            NumberAnimation {
                target: popupCard
                property: "opacity"
                to: 0
                duration: Theme.popupCloseDuration
                easing.type: Easing.InQuad
            }
            NumberAnimation {
                target: popupCard
                property: "scale"
                to: 0.95
                duration: Theme.popupCloseDuration
                easing.type: Easing.InCubic
            }
        }
        ScriptAction {
            script: popupWindow.visible = false
        }
    }
}
