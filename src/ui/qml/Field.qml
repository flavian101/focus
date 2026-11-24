import QtQuick
import QtQuick.Controls.Basic
import FocusUi

TextField {
    id: root
    implicitHeight: 44
    color: Theme.text
    placeholderTextColor: Theme.textDim
    font.family: Theme.fontFamily
    font.pixelSize: 15
    leftPadding: 14
    rightPadding: 14
    selectByMouse: true
    background: Rectangle {
        radius: 12
        color: Theme.surfaceHi
        border.width: 1
        border.color: root.activeFocus ? Theme.accent : Theme.border
        Behavior on border.color { ColorAnimation { duration: Theme.fast } }
    }
}
