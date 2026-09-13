import QtQuick
import QtQuick.Window

Window {
    required property var widgetModel

    // qmllint disable unqualified
    readonly property int warningTrigger: missingWarningValue
    // qmllint enable unqualified

    visible: false
}
