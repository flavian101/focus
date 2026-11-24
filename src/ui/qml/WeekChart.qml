import QtQuick
import FocusUi

// Focus minutes per day. Bars grow in one after another, and animate again when data changes.
Card {
    id: root
    property var days: []
    readonly property real maxMinutes: {
        var m = 30
        for (var i = 0; i < days.length; i++) m = Math.max(m, days[i].minutes)
        return m
    }

    Txt { id: head; text: "This week"; strong: true; size: 16 }
    Txt { anchors.right: parent.right; text: "focus minutes per day"; dim: true; size: 12 }

    Row {
        id: bars
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: head.bottom
        anchors.bottom: parent.bottom
        anchors.topMargin: 12
        spacing: 12

        Repeater {
            model: root.days
            delegate: Item {
                id: col
                required property var modelData
                required property int index
                property bool ready: false
                width: (bars.width - bars.spacing * 6) / 7
                height: bars.height

                Timer { interval: 350 + col.index * 80; running: true; onTriggered: col.ready = true }

                readonly property real areaHeight: height - 22 - 24  // room for the day label and the value

                Rectangle {
                    id: bar
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.bottom: dayLabel.top
                    anchors.bottomMargin: 6
                    width: Math.min(46, parent.width)
                    radius: 10
                    height: col.ready && col.modelData.minutes > 0
                            ? Math.max(6, col.modelData.minutes / root.maxMinutes * col.areaHeight) : 0
                    color: col.modelData.today ? Theme.accent : Qt.alpha(Theme.accent, 0.38)
                    Behavior on height { NumberAnimation { duration: 800; easing.type: Easing.OutCubic } }
                    Behavior on color { ColorAnimation { duration: Theme.normal } }
                }
                Txt {
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.bottom: bar.top
                    anchors.bottomMargin: 4
                    text: col.modelData.minutes > 0 ? Math.round(col.modelData.minutes) + "m" : ""
                    size: 12
                    dim: true
                    opacity: col.ready ? 1 : 0
                    Behavior on opacity { NumberAnimation { duration: 500 } }
                }
                Txt {
                    id: dayLabel
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.bottom: parent.bottom
                    text: col.modelData.label
                    size: 12
                    dim: !col.modelData.today
                    strong: col.modelData.today
                }
            }
        }
    }
}
