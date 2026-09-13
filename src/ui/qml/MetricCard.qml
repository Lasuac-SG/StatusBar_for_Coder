import QtQuick
import StatusBar

Item {
    id: root

    required property string label
    required property string value
    property string unit: ""
    property color accent: Theme.foreground

    implicitHeight: Theme.metricCardHeight

    Rectangle {
        anchors.fill: parent
        radius: Theme.radiusCard
        color: Theme.cardBackground

        Column {
            anchors.fill: parent
            anchors.margins: Theme.spacingMedium
            spacing: Theme.spacingTiny

            Text {
                text: root.label
                color: Theme.foregroundMuted
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontXs
            }

            Text {
                text: root.unit.length > 0
                    ? root.value + " " + root.unit
                    : root.value
                color: root.accent
                font.family: Theme.fontMono
                font.pixelSize: Theme.fontLg
                font.weight: Theme.weightLight
            }
        }
    }
}
