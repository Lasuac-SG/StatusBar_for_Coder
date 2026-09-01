pragma Singleton
import QtQuick

QtObject {
    id: root

    readonly property string fontFamily: "Segoe UI Variable Display, Segoe UI Variable Text, Segoe UI, -apple-system, sans-serif"
    readonly property string fontMono: "Segoe UI Variable Text, Segoe UI, Cascadia Code, sans-serif"

    readonly property real baseBarHeight: 36

    readonly property int fontXs: Math.max(9, Math.round(baseBarHeight * 0.30))   // 11px
    readonly property int fontSm: Math.max(11, Math.round(baseBarHeight * 0.36))  // 13px
    readonly property int fontMd: Math.max(13, Math.round(baseBarHeight * 0.42))  // 15px
    readonly property int fontLg: Math.max(16, Math.round(baseBarHeight * 0.50))  // 18px
    readonly property int fontXl: Math.max(18, Math.round(baseBarHeight * 0.58))  // 21px

    readonly property int weightExtraLight: Font.ExtraLight
    readonly property int weightLight: Font.Light
    readonly property int weightNormal: Font.Normal
    readonly property int weightMedium: Font.Medium
    readonly property int weightDemiBold: Font.DemiBold
}
