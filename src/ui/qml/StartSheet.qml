import QtQuick
import QtQuick.Controls.Basic
import FocusUi

// The "contract" for a session: what, how you will start on your own, how long, and how much AI.
Sheet {
    id: root
    panelWidth: 560
    property bool unlimited: false
    property int budget: 3
    property string error: ""
    onDismissed: open = false

    // However a session starts (this sheet, the tray, a test), the sheet must get out of the way.
    Connections {
        target: backend
        function onChanged() { if (backend.phase !== "idle") root.open = false }
    }

    function reset() {
        goal.text = ""
        approach.text = ""
        tag.text = ""
        mood.value = 3
        energy.value = 3
        error = ""
    }

    Column {
        width: parent.width
        spacing: 12

        Txt { text: "Start a session"; size: 24; strong: true }

        Txt { text: "What will you get done?"; dim: true; size: 13 }
        Field { id: goal; width: parent.width; placeholderText: "Be specific, e.g. finish the login endpoint" }

        Txt { text: "What will you try on your own first, before asking AI?"; dim: true; size: 13 }
        Field { id: approach; width: parent.width; placeholderText: "e.g. read the error, reproduce it, check the docs" }

        Row {
            spacing: 16
            Txt { anchors.verticalCenter: parent.verticalCenter; width: 70; text: "Length"; dim: true; size: 13 }
            Slider {
                id: length
                anchors.verticalCenter: parent.verticalCenter
                width: 300
                from: 5; to: 120; stepSize: 5; value: 25
                background: Rectangle {
                    x: length.leftPadding; y: length.topPadding + length.availableHeight / 2 - height / 2
                    width: length.availableWidth; height: 6; radius: 3; color: Theme.surfaceHi
                    Rectangle { width: length.visualPosition * parent.width; height: parent.height; radius: 3; color: Theme.accent }
                }
                handle: Rectangle {
                    x: length.leftPadding + length.visualPosition * (length.availableWidth - width)
                    y: length.topPadding + length.availableHeight / 2 - height / 2
                    width: 22; height: 22; radius: 11; color: "white"
                    border.width: 3; border.color: Theme.accent
                    scale: length.pressed ? 1.2 : 1.0
                    Behavior on scale { NumberAnimation { duration: Theme.fast } }
                }
            }
            Txt { anchors.verticalCenter: parent.verticalCenter; text: length.value + " min"; strong: true; size: 14 }
        }

        Row {
            spacing: 12
            Txt { anchors.verticalCenter: parent.verticalCenter; width: 70 + 4; text: "AI budget"; dim: true; size: 13 }
            PillButton { text: "−"; enabled: !root.unlimited && root.budget > 0; onClicked: root.budget-- }
            Txt {
                anchors.verticalCenter: parent.verticalCenter
                width: 64; horizontalAlignment: Text.AlignHCenter
                text: root.unlimited ? "no limit" : root.budget + (root.budget === 1 ? " visit" : " visits")
                strong: true; size: 14
            }
            PillButton { text: "+"; enabled: !root.unlimited && root.budget < 20; onClicked: root.budget++ }
            PillButton { text: root.unlimited ? "Set a limit" : "No limit"; onClicked: root.unlimited = !root.unlimited }
        }

        Row {
            spacing: 24
            Column {
                spacing: 6
                Txt { text: "Mood right now"; dim: true; size: 13 }
                MoodPicker { id: mood; size: 38 }
            }
            Column {
                spacing: 6
                Txt { text: "Energy"; dim: true; size: 13 }
                MoodPicker { id: energy; size: 38 }
            }
        }
        Field { id: tag; width: parent.width; placeholderText: "One word for the feeling (optional): anxious, bored, stuck..." }

        Txt { visible: root.error !== ""; text: root.error; color: Theme.bad; size: 13 }

        Row {
            anchors.right: parent.right
            spacing: 10
            PillButton { text: "Cancel"; onClicked: root.open = false }
            PillButton {
                primary: true
                text: "Start focusing"
                enabled: goal.text.trim() !== "" && approach.text.trim() !== ""
                onClicked: {
                    var ok = backend.startSession(goal.text, approach.text, length.value,
                                                  root.unlimited ? -1 : root.budget, mood.value, energy.value, tag.text)
                    if (ok) { root.open = false; root.reset() }
                    else root.error = "Could not start. A session may already be running."
                }
            }
        }
    }
}
