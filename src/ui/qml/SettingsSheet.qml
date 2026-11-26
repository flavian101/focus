import QtQuick
import FocusUi

// Appearance, a few words about you, and how much detail tracking keeps (and when).
Sheet {
    id: root
    panelWidth: 600
    onDismissed: open = false

    // working copy of the schedule, applied on Save
    property int daysMask: 31
    property int startMinute: 540
    property int endMinute: 1020
    property string workMode: "work"
    property string otherMode: "private"
    property string error: ""

    function clock(minute) {
        var h = Math.floor(minute / 60), m = minute % 60
        return (h < 10 ? "0" : "") + h + ":" + (m < 10 ? "0" : "") + m
    }
    function load() {
        var p = backend.trackingPolicy
        daysMask = p.daysMask !== undefined ? p.daysMask : 31
        startMinute = p.startMinute !== undefined ? p.startMinute : 540
        endMinute = p.endMinute !== undefined ? p.endMinute : 1020
        workMode = p.workMode || "work"
        otherMode = p.otherMode || "private"
        roleField.text = backend.profile.role || ""
        purposeField.text = backend.profile.purpose || ""
        error = ""
    }
    onOpenChanged: if (open) load()

    readonly property var dayNames: ["Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"]
    readonly property var modes: [{ key: "work", label: "Full detail" }, { key: "private", label: "Category only" }, { key: "off", label: "Off" }]

    Column {
        width: parent.width
        spacing: 12

        Txt { text: "Settings"; size: 24; strong: true }

        // ---- appearance ------------------------------------------------------------------
        Row {
            spacing: 10
            Txt { anchors.verticalCenter: parent.verticalCenter; width: 92; text: "Appearance"; dim: true; size: 13 }
            Repeater {
                model: ["#7C5CFF", "#3B82F6", "#14B8A6", "#F59E0B", "#F43F5E", "#A3E635"]
                delegate: Item {
                    required property string modelData
                    width: 28; height: 28
                    anchors.verticalCenter: parent.verticalCenter
                    scale: swatchArea.containsMouse ? 1.18 : 1.0
                    Behavior on scale { NumberAnimation { duration: Theme.fast } }
                    Rectangle {
                        anchors.fill: parent; radius: 14; color: modelData
                        border.width: backend.accent.toLowerCase() === modelData.toLowerCase() ? 3 : 0
                        border.color: Theme.text
                    }
                    MouseArea { id: swatchArea; anchors.fill: parent; hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor; onClicked: backend.accent = modelData }
                }
            }
            PillButton { implicitHeight: 34; text: backend.darkMode ? "Light theme" : "Dark theme"; onClicked: backend.darkMode = !backend.darkMode }
        }

        // ---- about you -------------------------------------------------------------------
        Txt { text: "About you"; strong: true; size: 15 }
        Field { id: roleField; width: parent.width; placeholderText: "Your role, e.g. software engineering intern" }
        Field { id: purposeField; width: parent.width; placeholderText: "What is this computer for? e.g. mostly coding; the browser is for docs" }

        // ---- tracking ------------------------------------------------------------------------
        Txt { text: "Tracking"; strong: true; size: 15 }
        Txt { text: "Work days"; dim: true; size: 13 }
        Row {
            spacing: 8
            Repeater {
                model: 7
                delegate: Rectangle {
                    id: dayChip
                    required property int index
                    readonly property bool on: (root.daysMask & (1 << index)) !== 0
                    width: 54; height: 32; radius: 16
                    color: on ? Qt.alpha(Theme.accent, 0.2) : "transparent"
                    border.width: 1; border.color: on ? Theme.accent : Theme.border
                    Behavior on color { ColorAnimation { duration: Theme.fast } }
                    Behavior on border.color { ColorAnimation { duration: Theme.fast } }
                    Txt { anchors.centerIn: parent; text: root.dayNames[index]; size: 13; strong: true
                          color: dayChip.on ? Theme.accent : Theme.textDim }
                    MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                                onClicked: root.daysMask = root.daysMask ^ (1 << dayChip.index) }
                }
            }
        }
        Row {
            spacing: 10
            Txt { anchors.verticalCenter: parent.verticalCenter; width: 92; text: "Work hours"; dim: true; size: 13 }
            PillButton { implicitHeight: 32; text: "−"; enabled: root.startMinute >= 30; onClicked: root.startMinute -= 30 }
            Txt { anchors.verticalCenter: parent.verticalCenter; width: 46; horizontalAlignment: Text.AlignHCenter; text: root.clock(root.startMinute); strong: true; size: 14 }
            PillButton { implicitHeight: 32; text: "+"; enabled: root.startMinute + 30 < root.endMinute; onClicked: root.startMinute += 30 }
            Txt { anchors.verticalCenter: parent.verticalCenter; text: "to"; dim: true; size: 13 }
            PillButton { implicitHeight: 32; text: "−"; enabled: root.endMinute - 30 > root.startMinute; onClicked: root.endMinute -= 30 }
            Txt { anchors.verticalCenter: parent.verticalCenter; width: 46; horizontalAlignment: Text.AlignHCenter; text: root.clock(root.endMinute); strong: true; size: 14 }
            PillButton { implicitHeight: 32; text: "+"; enabled: root.endMinute + 30 <= 1440; onClicked: root.endMinute += 30 }
        }

        Repeater {
            model: [{ label: "During work hours", prop: "workMode" }, { label: "Outside work hours", prop: "otherMode" }]
            delegate: Row {
                id: modeRow
                required property var modelData
                spacing: 8
                Txt { anchors.verticalCenter: parent.verticalCenter; width: 130; text: modeRow.modelData.label; dim: true; size: 13 }
                Repeater {
                    model: root.modes
                    delegate: Rectangle {
                        id: modeChip
                        required property var modelData
                        readonly property bool selected: root[modeRow.modelData.prop] === modelData.key
                        height: 32; width: modeLabel.implicitWidth + 28; radius: 16
                        color: selected ? Qt.alpha(Theme.accent, 0.2) : "transparent"
                        border.width: 1; border.color: selected ? Theme.accent : Theme.border
                        Behavior on color { ColorAnimation { duration: Theme.fast } }
                        Behavior on border.color { ColorAnimation { duration: Theme.fast } }
                        Txt { id: modeLabel; anchors.centerIn: parent; text: modeChip.modelData.label; size: 13; strong: true
                              color: modeChip.selected ? Theme.accent : Theme.textDim }
                        MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                                    onClicked: root[modeRow.modelData.prop] = modeChip.modelData.key }
                    }
                }
            }
        }
        Txt {
            width: parent.width; wrapMode: Text.Wrap; size: 12; dim: true
            text: "Full detail keeps the app name (code, chrome). Category only keeps just \"40 minutes of distraction\", never which app. Everything stays on this computer; window titles and typing are never stored."
        }

        Txt { visible: root.error !== ""; text: root.error; color: Theme.bad; size: 13 }
        Row {
            anchors.right: parent.right
            spacing: 10
            PillButton { text: "Cancel"; onClicked: root.open = false }
            PillButton {
                primary: true
                text: "Save"
                onClicked: {
                    var ok = backend.setTrackingPolicy(root.workMode, root.otherMode, root.daysMask, root.startMinute, root.endMinute)
                    if (!ok) { root.error = "Those work hours do not make sense."; return }
                    backend.setProfile(roleField.text, purposeField.text)
                    root.open = false
                }
            }
        }
    }
}
