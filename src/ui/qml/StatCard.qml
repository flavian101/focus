import QtQuick
import FocusUi

// A headline number that counts up to its value, with a title and a line of detail.
Card {
    id: root
    property string title
    property real value: 0
    property var formatter: function (v) { return Math.round(v).toString() }
    property string sub
    property color tint: Theme.accent
    property real shown: 0

    Behavior on shown { NumberAnimation { duration: 900; easing.type: Easing.OutCubic } }
    onValueChanged: shown = value
    Component.onCompleted: shown = value

    Rectangle { width: 28; height: 4; radius: 2; color: root.tint
        Behavior on color { ColorAnimation { duration: Theme.normal } } }
    Txt { y: 14; text: root.title; dim: true; size: 13 }
    Txt { y: 36; text: root.formatter(root.shown); size: 34; strong: true }
    Txt { y: 84; width: parent.width; text: root.sub; dim: true; size: 12; elide: Text.ElideRight }
}
