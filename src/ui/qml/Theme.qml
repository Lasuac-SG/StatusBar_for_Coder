pragma Singleton

import QtQuick

QtObject {
    readonly property string fontFamily: "Segoe UI Variable Text"
    readonly property string fontMono: "Cascadia Mono"

    readonly property real baseBarHeight: 40
    readonly property real outerMarginRatio: 0.3
    readonly property real barRadiusRatio: 0.25
    readonly property real widgetRadiusRatio: 0.6
    readonly property real editingBorderWidth: 1.5
    readonly property real metricCardHeight: 64

    readonly property int spacingTiny: 2
    readonly property int spacingSmall: 4
    readonly property int spacingMedium: 10
    readonly property int spacingLarge: 12
    readonly property int spacingExtraLarge: 16

    readonly property int radiusControl: 6
    readonly property int radiusCard: 8
    readonly property int radiusPopup: 12

    readonly property int hoverAnimationDuration: 150
    readonly property int popupOpenDuration: 180
    readonly property int popupCloseDuration: 140
    readonly property int layoutAnimationDuration: 250

    readonly property int fontXs: Math.max(9, Math.round(baseBarHeight * 0.28))
    readonly property int fontSm: Math.max(11, Math.round(baseBarHeight * 0.33))
    readonly property int fontLg: Math.max(16, Math.round(baseBarHeight * 0.45))

    readonly property int weightExtraLight: Font.ExtraLight
    readonly property int weightLight: Font.Light

    readonly property color foreground: "#f0f0f0"
    readonly property color foregroundMuted: "#98989f"
    readonly property color foregroundSubtle: "#636366"
    readonly property color barBackground: "#1e1e1e"
    readonly property color popupBackground: "#1c1c1e"
    readonly property color chartBackground: "#161618"
    readonly property color cardBackground: "#252528"
    readonly property color border: "#2c2c2e"
    readonly property color chartGrid: "#242428"
    readonly property color accent: "#0084ff"
    readonly property color accentStrong: "#0078d4"
    readonly property color error: "#ff5555"
    readonly property color editingBorder: "#8be9fd"
    readonly property color hoverBackground: "#14ffffff"
    readonly property color dragBackground: "#1affffff"
    readonly property color chartFill: "#550084ff"
}
