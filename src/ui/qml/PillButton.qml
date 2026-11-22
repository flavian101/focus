import QtQuick
import FocusUi

Item {
    id: root
    property string text
    property bool primary: false
    property bool danger: false
    property color tint: Theme.accent
    signal clicked()

    implicitWidth: label.implicitWidth + 36
    implicitHeight: 42
    opacity: enabled ? 1 : 0.4
    scale: mouse.pressed ? 0.96 : (mouse.containsMouse ? 1.03 : 1.0)
    Behavior on scale { NumberAnimation { duration: Theme.fast; easing.type: Easing.OutCubic } }

    Rectangle {
        anchors.fill: parent
        radius: height / 2
        color: root.primary ? root.tint : (mouse.containsMouse ? Theme.surfaceHi : "transparent")
        border.width: root.primary ? 0 : 1
        border.color: root.danger ? Theme.bad : Theme.border
        Behavior on color { ColorAnimation { duration: Theme.fast } }
    }
    Txt {
        id: label
        anchors.centerIn: parent
        text: root.text
        size: 14
        strong: true
        color: root.primary ? "white" : (root.danger ? Theme.bad : Theme.text)
    }
    MouseArea {
        id: mouse
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: root.clicked()
    }
}
