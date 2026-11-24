import QtQuick
import FocusUi

// Text with the app's font and colours, so screens only say what they mean.
Text {
    property bool dim: false
    property int size: 14
    property bool strong: false

    color: dim ? Theme.textDim : Theme.text
    font.family: Theme.fontFamily
    font.pixelSize: size
    font.weight: strong ? Font.DemiBold : Font.Normal
    Behavior on color { ColorAnimation { duration: Theme.normal } }
}
