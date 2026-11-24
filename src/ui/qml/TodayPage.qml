import QtQuick
import QtQuick.Layouts
import FocusUi

// Plan the day: add tasks with a priority and an estimate, tick them off, start a focus session on one.
Item {
    id: root
    property bool active: true
    signal focusTask(int taskId, string title, int minutes)

    opacity: active ? 1 : 0
    visible: opacity > 0.001
    Behavior on opacity { NumberAnimation { duration: Theme.normal; easing.type: Easing.OutCubic } }
    transform: Translate { y: root.active ? 0 : -24; Behavior on y { NumberAnimation { duration: 400; easing.type: Easing.OutCubic } } }

    readonly property var plan: backend.plan
    property int newPriority: 2
    property int newMinutes: 30
    property string error: ""

    function minutesText(v) {
        return v >= 60 ? Math.floor(v / 60) + "h" + (Math.round(v % 60) > 0 ? " " + Math.round(v % 60) + "m" : "") : Math.round(v) + "m"
    }

    function addTask() {
        var err = backend.addTask(titleField.text, root.newPriority, root.newMinutes)
        if (err === "") { titleField.text = ""; root.error = "" } else root.error = err
    }

    // Keep the on-screen list in step with the backend WITHOUT rebuilding it, so rows animate
    // in, slide to their new place and fade out instead of the whole list flashing.
    ListModel { id: taskModel }
    function syncTasks() {
        var list = backend.tasks
        for (var i = taskModel.count - 1; i >= 0; i--) {              // 1) drop rows that are gone
            var found = false
            for (var j = 0; j < list.length; j++) if (list[j].taskId === taskModel.get(i).taskId) { found = true; break }
            if (!found) taskModel.remove(i)
        }
        for (var k = 0; k < list.length; k++) {                        // 2) match order and contents
            var item = list[k]
            if (k < taskModel.count && taskModel.get(k).taskId === item.taskId) {
                taskModel.set(k, item)
            } else {
                var from = -1
                for (var m = k + 1; m < taskModel.count; m++) if (taskModel.get(m).taskId === item.taskId) { from = m; break }
                if (from >= 0) { taskModel.move(from, k, 1); taskModel.set(k, item) }
                else taskModel.insert(k, item)
            }
        }
    }
    Connections { target: backend; function onTasksChanged() { root.syncTasks() } }
    Component.onCompleted: syncTasks()

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 36
        anchors.rightMargin: 36
        anchors.topMargin: 6
        anchors.bottomMargin: 36
        spacing: 18

        // ================================================================== the plan
        Card {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.preferredWidth: 3
            hoverable: false
            padding: 22

            Item {
                id: head
                width: parent.width; height: 30
                Txt { text: "Today's plan"; size: 20; strong: true }
                Txt {
                    anchors.right: parent.right; anchors.verticalCenter: parent.verticalCenter
                    text: (root.plan.done || 0) + " of " + (root.plan.total || 0) + " done"
                    dim: true; size: 14
                }
            }

            Column {
                id: form
                anchors.top: head.bottom
                anchors.topMargin: 14
                width: parent.width
                spacing: 10

                Row {
                    spacing: 10
                    Field {
                        id: titleField
                        width: form.width - addButton.width - 10
                        placeholderText: "What do you want to get done today?"
                        onAccepted: root.addTask()
                    }
                    PillButton { id: addButton; implicitHeight: 44; primary: true; text: "Add"; onClicked: root.addTask() }
                }

                Row {
                    spacing: 18
                    Row {
                        spacing: 8
                        anchors.verticalCenter: parent.verticalCenter
                        Repeater {
                            model: [1, 2, 3]
                            delegate: Rectangle {
                                id: chip
                                required property int modelData
                                readonly property bool selected: root.newPriority === modelData
                                readonly property color tone: Theme.priorityColor(modelData)
                                height: 32; width: chipText.implicitWidth + 28; radius: 16
                                color: selected ? Qt.alpha(tone, 0.18) : "transparent"
                                border.width: 1
                                border.color: selected ? tone : Theme.border
                                Behavior on color { ColorAnimation { duration: Theme.fast } }
                                Behavior on border.color { ColorAnimation { duration: Theme.fast } }
                                Txt {
                                    id: chipText; anchors.centerIn: parent; size: 13; strong: true
                                    color: chip.selected ? chip.tone : Theme.textDim
                                    text: modelData === 1 ? "Must " + (root.plan.mustTotal || 0) + "/" + (root.plan.maxMust || 3)
                                                          : (modelData === 2 ? "Should" : "Could")
                                }
                                MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                                            onClicked: root.newPriority = chip.modelData }
                            }
                        }
                    }
                    Row {
                        spacing: 8
                        anchors.verticalCenter: parent.verticalCenter
                        PillButton { implicitHeight: 32; text: "−"; enabled: root.newMinutes > 5; onClicked: root.newMinutes -= 5 }
                        Txt { anchors.verticalCenter: parent.verticalCenter; width: 62; horizontalAlignment: Text.AlignHCenter
                              text: root.newMinutes + " min"; strong: true; size: 14 }
                        PillButton { implicitHeight: 32; text: "+"; enabled: root.newMinutes < 240; onClicked: root.newMinutes += 5 }
                    }
                }

                Txt {
                    width: parent.width
                    visible: root.error !== ""
                    text: root.error; color: Theme.warn; size: 13; wrapMode: Text.Wrap
                }
            }

            ListView {
                id: list
                anchors.top: form.bottom
                anchors.topMargin: 12
                anchors.bottom: parent.bottom
                width: parent.width
                clip: true
                spacing: 2
                model: taskModel

                add: Transition {
                    ParallelAnimation {
                        NumberAnimation { property: "opacity"; from: 0; to: 1; duration: 350 }
                        NumberAnimation { property: "x"; from: -28; to: 0; duration: 380; easing.type: Easing.OutCubic }
                    }
                }
                remove: Transition { NumberAnimation { property: "opacity"; to: 0; duration: 200 } }
                displaced: Transition { NumberAnimation { properties: "y"; duration: 340; easing.type: Easing.OutCubic } }
                move: Transition { NumberAnimation { properties: "y"; duration: 340; easing.type: Easing.OutCubic } }

                delegate: TaskRow {
                    onToggled: (id, done) => backend.setTaskDone(id, done)
                    onFocusRequested: (id, title, minutes) => root.focusTask(id, title, minutes)
                    onPriorityClicked: (id) => { root.error = backend.cyclePriority(id) }
                    onDropRequested: (id) => backend.dropTask(id)
                    onRestoreRequested: (id) => backend.restoreTask(id)
                }

                Txt {
                    anchors.centerIn: parent
                    width: parent.width - 60
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.Wrap
                    visible: taskModel.count === 0
                    text: "Nothing planned yet. Start with your must-dos. You only get " + (root.plan.maxMust || 3) + "."
                    dim: true; size: 15
                }
            }
        }

        // ================================================================== at a glance
        ColumnLayout {
            Layout.preferredWidth: 330
            Layout.maximumWidth: 330
            Layout.fillHeight: true
            spacing: 18

            Card {
                Layout.fillWidth: true
                Layout.preferredHeight: 398
                hoverable: false
                enterDelay: 120

                Txt { text: "Today at a glance"; size: 16; strong: true }

                ProgressRing {
                    id: ring
                    y: 40
                    anchors.horizontalCenter: parent.horizontalCenter
                    width: 150; height: 150; thickness: 14
                    progress: root.plan.completion || 0
                    ringColor: Theme.good
                    Column {
                        anchors.centerIn: parent
                        spacing: 2
                        Txt { anchors.horizontalCenter: parent.horizontalCenter; text: (root.plan.done || 0) + "/" + (root.plan.total || 0)
                              size: 34; strong: true }
                        Txt { anchors.horizontalCenter: parent.horizontalCenter; text: "tasks done"; dim: true; size: 12 }
                    }
                }

                Column {
                    anchors.top: ring.bottom
                    anchors.topMargin: 22
                    width: parent.width
                    spacing: 14

                    Item {
                        width: parent.width; height: 20
                        Txt { text: "Must-dos"; dim: true; size: 13 }
                        Txt { anchors.right: parent.right; size: 13; strong: true
                              text: (root.plan.mustDone || 0) + " of " + (root.plan.mustTotal || 0) + " done" }
                    }
                    Item {
                        width: parent.width; height: 20
                        Txt { text: "Planned"; dim: true; size: 13 }
                        Txt { anchors.right: parent.right; size: 13; strong: true
                              text: root.minutesText(root.plan.plannedMinutes || 0) + " of " + root.minutesText(root.plan.capacityMinutes || 240) }
                    }
                    Rectangle {
                        width: parent.width; height: 8; radius: 4; color: Theme.surfaceHi
                        Rectangle {
                            height: parent.height; radius: 4
                            width: Math.min(1, (root.plan.plannedMinutes || 0) / Math.max(1, root.plan.capacityMinutes || 240)) * parent.width
                            color: root.plan.overcommitted ? Theme.warn : Theme.accent
                            Behavior on width { NumberAnimation { duration: 700; easing.type: Easing.OutCubic } }
                            Behavior on color { ColorAnimation { duration: Theme.normal } }
                        }
                    }
                    Txt {
                        width: parent.width
                        visible: root.plan.overcommitted === true
                        text: "That is more than a day holds. Drop or defer something."
                        color: Theme.warn; size: 12; wrapMode: Text.Wrap
                    }
                    Item {
                        width: parent.width; height: 20
                        Txt { text: "Focused on these"; dim: true; size: 13 }
                        Txt { anchors.right: parent.right; size: 13; strong: true; text: root.minutesText(root.plan.actualMinutes || 0) }
                    }
                }
            }

            Card {
                Layout.fillWidth: true
                Layout.preferredHeight: 132
                visible: backend.carryOverCount > 0
                hoverable: false
                enterDelay: 220

                Txt { id: carryTitle; text: "Unfinished from earlier days"; size: 15; strong: true }
                Txt {
                    anchors.top: carryTitle.bottom; anchors.topMargin: 4
                    text: backend.carryOverCount + (backend.carryOverCount === 1 ? " task is" : " tasks are") + " still open."
                    dim: true; size: 13
                }
                PillButton {
                    anchors.bottom: parent.bottom
                    text: "Bring them to today"
                    onClicked: backend.carryOver()
                }
            }

            Item { Layout.fillHeight: true }
        }
    }
}
