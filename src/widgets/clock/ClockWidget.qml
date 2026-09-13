import QtQuick
import QtQuick.Window
import StatusBar

Item {
    id: root

    required property ClockViewModel viewModel
    property Window editingWindow: null
    property bool editing: false
    property var requestEditing: null

    Text {
        anchors.centerIn: parent
        text: root.viewModel ? root.viewModel.timeText : "--:--"
        color: Theme.foreground
        font.family: Theme.fontFamily
        font.pixelSize: Theme.fontSm
        font.weight: Theme.weightLight
    }
}
