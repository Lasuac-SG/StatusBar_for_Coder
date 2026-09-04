import QtQuick
import Theme 1.0

Item {
    id: root
    anchors.fill: parent
    property var viewModel

    Text {
        anchors.centerIn: parent
        text: root.viewModel ? root.viewModel.timeText : "--:--"
        color: "#f0f0f0"
        font.family: Theme.fontFamily
        font.pixelSize: Theme.fontSm
        font.weight: Theme.weightLight
        renderType: Text.NativeRendering
    }
}


