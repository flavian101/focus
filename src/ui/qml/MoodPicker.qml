import QtQuick
import FocusUi

// Five numbered circles, 1 (low) to 5 (high). The chosen one pops and takes its mood colour.
Row {
    id: root
    property int value: 3
    property int size: 46
    spacing: Math.round(size * 0.22)

    Repeater {
        model: 5
        delegate: Item {
            id: dot
            required property int index
            readonly property int n: index + 1
            readonly property bool selected: root.value === n
            width: root.size
            height: root.size
            scale: selected ? 1.14 : (area.containsMouse ? 1.06 : 1.0)
            Behavior on scale { NumberAnimation { duration: 220; easing.type: Easing.OutBack } }

            Rectangle {
                anchors.fill: parent
                radius: width / 2
                color: dot.selected ? Theme.moodColor(dot.n) : Theme.surfaceHi
                border.width: 1
                border.color: dot.selected ? "transparent" : Theme.border
                Behavior on color { ColorAnimation { duration: Theme.normal } }
            }
            Txt {
                anchors.centerIn: parent
                text: dot.n
                size: 16
                strong: true
                color: dot.selected ? "white" : Theme.textDim
            }
            MouseArea {
                id: area
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: root.value = dot.n
            }
        }
    }
}
