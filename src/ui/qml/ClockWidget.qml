import QtQuick

Item {
    Text {
        anchors.centerIn: parent
        text: clockAdapter.timeText
        color: "#FFFFFF"
        
        font.family: "Segoe UI"
        font.pixelSize: Math.round(15 * Screen.devicePixelRatio)
        font.weight: Font.Normal
        
        // 关键设置：强制使用操作系统的原生字体渲染引擎 (DirectWrite)，保障 ClearType 的锐利度
        renderType: Text.NativeRendering
    }
}
