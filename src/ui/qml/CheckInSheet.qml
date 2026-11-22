import QtQuick
import FocusUi

// Shown when a session ends. The session is already saved; this adds how you felt afterwards.
Sheet {
    id: root
    open: backend.phase === "checkin"
    panelWidth: 520
    onDismissed: backend.skipCheckIn()

    readonly property var s: backend.summary

    Column {
        width: parent.width
        spacing: 14

        Txt { text: root.s.completed ? "Session complete" : "Session ended early"; size: 24; strong: true }
        Txt { width: parent.width; text: root.s.goal ? root.s.goal : ""; dim: true; size: 14; elide: Text.ElideRight }

        Row {
            spacing: 10
            Repeater {
                model: [
                    { text: (root.s.activeMinutes !== undefined ? root.s.activeMinutes : 0) + " of "
                            + (root.s.plannedMinutes !== undefined ? root.s.plannedMinutes : 0) + " min focused", tone: "good" },
                    { text: "AI visits: " + (root.s.aiVisits !== undefined ? root.s.aiVisits : 0)
                            + (root.s.aiBudget >= 0 ? " of " + root.s.aiBudget : ""),
                      tone: (root.s.aiBudget >= 0 && root.s.aiVisits > root.s.aiBudget) ? "bad" : "ai" },
                    { text: "Away " + (root.s.awayMinutes !== undefined ? root.s.awayMinutes : 0) + " min", tone: "dim" }
                ]
                delegate: Rectangle {
                    required property var modelData
                    height: 30
                    width: chipText.implicitWidth + 24
                    radius: 15
                    color: Theme.surfaceHi
                    border.width: 1
                    border.color: modelData.tone === "good" ? Qt.alpha(Theme.good, 0.6)
                                 : modelData.tone === "bad" ? Qt.alpha(Theme.bad, 0.7)
                                 : modelData.tone === "ai" ? Qt.alpha(Theme.cAi, 0.6) : Theme.border
                    Txt { id: chipText; anchors.centerIn: parent; text: modelData.text; size: 12 }
                }
            }
        }

        Txt { text: "How was that?"; strong: true; size: 16 }
        Row {
            spacing: 24
            Column {
                spacing: 6
                Txt { text: "Mood"; dim: true; size: 13 }
                MoodPicker { id: mood; size: 38 }
            }
            Column {
                spacing: 6
                Txt { text: "Energy"; dim: true; size: 13 }
                MoodPicker { id: energy; size: 38 }
            }
        }
        Field { id: tag; width: parent.width; placeholderText: "One word for the feeling (optional)" }

        Row {
            anchors.right: parent.right
            spacing: 10
            PillButton { text: "Skip"; onClicked: backend.skipCheckIn() }
            PillButton {
                primary: true
                text: "Save check-in"
                onClicked: { backend.submitCheckIn(mood.value, energy.value, tag.text); tag.text = ""; mood.value = 3; energy.value = 3 }
            }
        }
    }
}
