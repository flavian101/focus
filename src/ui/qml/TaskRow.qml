import QtQuick
import QtQuick.Shapes
import FocusUi

// One planned task: a check circle, the title, a priority chip you can click to cycle,
// the time estimate vs time spent, and a button to start focusing on it.
Item {
    id: row
    required property int taskId
    required property string title
    required property int priority
    required property string priorityLabel
    required property int estimate
    required property int actualMinutes
    required property bool done
    required property bool dropped

    signal toggled(int taskId, bool done)
    signal focusRequested(int taskId, string title, int minutes)
    signal priorityClicked(int taskId)
    signal dropRequested(int taskId)
    signal restoreRequested(int taskId)

    readonly property bool finished: done || dropped
    width: ListView.view ? ListView.view.width : 400
    height: 64

    Rectangle {
        anchors.fill: parent
        anchors.topMargin: 2; anchors.bottomMargin: 2
        radius: 14
        color: hover.hovered ? Theme.surfaceHi : "transparent"
        Behavior on color { ColorAnimation { duration: Theme.fast } }
    }
    HoverHandler { id: hover }

    // ---- check circle ------------------------------------------------------------------
    Item {
        id: check
        x: 10; anchors.verticalCenter: parent.verticalCenter
        width: 28; height: 28
        enabled: !row.dropped
        scale: checkArea.pressed ? 0.88 : 1.0
        Behavior on scale { NumberAnimation { duration: Theme.fast; easing.type: Easing.OutBack } }
        Rectangle {
            anchors.fill: parent; radius: 14
            color: row.done ? Theme.good : "transparent"
            border.width: 2
            border.color: row.done ? Theme.good : (row.dropped ? Theme.border : Theme.textDim)
            Behavior on color { ColorAnimation { duration: 220 } }
            Behavior on border.color { ColorAnimation { duration: 220 } }
        }
        Shape {
            anchors.centerIn: parent
            width: 14; height: 14
            opacity: row.done ? 1 : 0
            scale: row.done ? 1 : 0.3
            Behavior on opacity { NumberAnimation { duration: 160 } }
            Behavior on scale { NumberAnimation { duration: 320; easing.type: Easing.OutBack } }
            ShapePath {
                strokeColor: "white"; strokeWidth: 2.6; fillColor: "transparent"
                capStyle: ShapePath.RoundCap; joinStyle: ShapePath.RoundJoin
                startX: 1.5; startY: 7.5
                PathLine { x: 5.5; y: 11.5 }
                PathLine { x: 12.5; y: 3.5 }
            }
        }
        MouseArea { id: checkArea; anchors.fill: parent; cursorShape: Qt.PointingHandCursor
            onClicked: row.toggled(row.taskId, !row.done) }
    }

    // ---- title and details ----------------------------------------------------------------
    Column {
        anchors.verticalCenter: parent.verticalCenter
        x: 54
        width: parent.width - 54 - actions.width - 20
        spacing: 5
        Txt {
            width: parent.width
            text: row.title; size: 15; strong: !row.finished
            elide: Text.ElideRight
            font.strikeout: row.finished
            opacity: row.finished ? 0.5 : 1
            Behavior on opacity { NumberAnimation { duration: 250 } }
        }
        Row {
            spacing: 10
            Rectangle {
                id: chip
                height: 20; width: chipLabel.implicitWidth + 20; radius: 10
                color: "transparent"
                border.width: 1
                border.color: Theme.priorityColor(row.priority)
                opacity: row.finished ? 0.45 : 1
                Behavior on border.color { ColorAnimation { duration: Theme.normal } }
                Txt { id: chipLabel; anchors.centerIn: parent; text: row.priorityLabel; size: 11; strong: true
                      color: Theme.priorityColor(row.priority) }
                MouseArea { anchors.fill: parent; enabled: !row.finished; cursorShape: Qt.PointingHandCursor
                            onClicked: row.priorityClicked(row.taskId) }
            }
            Txt {
                text: row.actualMinutes > 0 ? row.actualMinutes + " of " + row.estimate + " min" : row.estimate + " min"
                dim: true; size: 12
                anchors.verticalCenter: parent.verticalCenter
            }
        }
    }

    // ---- actions -----------------------------------------------------------------------------
    Row {
        id: actions
        anchors.right: parent.right; anchors.rightMargin: 12
        anchors.verticalCenter: parent.verticalCenter
        spacing: 8
        PillButton {
            visible: !row.finished
            implicitHeight: 34
            primary: true; text: "Focus"
            onClicked: row.focusRequested(row.taskId, row.title, Math.max(5, row.estimate - row.actualMinutes))
        }
        PillButton {
            visible: !row.done && !row.dropped
            implicitHeight: 34
            text: "Drop"
            onClicked: row.dropRequested(row.taskId)
        }
        PillButton {
            visible: row.dropped
            implicitHeight: 34
            text: "Restore"
            onClicked: row.restoreRequested(row.taskId)
        }
    }
}
