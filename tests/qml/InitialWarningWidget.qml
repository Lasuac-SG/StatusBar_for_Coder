import QtQuick
import QtQuick.Window
import StatusBar

Item {
    id: root

    required property WidgetViewModel viewModel
    required property Window editingWindow
    required property bool editing
    required property var requestEditing

    Component.onCompleted: Qt.createQmlObject(
        "import QtQuick; QtObject { readonly property int warningTrigger: missingInitialValue }",
        root,
        "initialWarningFixture")
}
