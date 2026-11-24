import QtQuick
import QtQuick.Layouts
import FocusUi

// Greeting, the Today / Insights switch, appearance controls and the main call to action.
Item {
    id: root
    property string tab: "today"
    signal tabSelected(string name)
    signal startRequested()

    implicitHeight: 96

    function greeting() {
        var h = new Date().getHours()
        return h < 12 ? "Good morning" : (h < 18 ? "Good afternoon" : "Good evening")
    }
    function dateText() {
        var p = backend.today.split("-")
        if (p.length !== 3) return ""
        var d = new Date(parseInt(p[0]), parseInt(p[1]) - 1, parseInt(p[2]))
        return Qt.formatDate(d, "dddd, d MMMM")
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 36
        anchors.rightMargin: 36
        anchors.topMargin: 22
        anchors.bottomMargin: 8
        spacing: 20

        Column {
            Layout.alignment: Qt.AlignVCenter
            spacing: 2
            Txt { text: root.greeting(); size: 28; strong: true }
            Txt { text: root.tab === "insights" ? "Your last 7 days" : root.dateText(); dim: true; size: 14 }
        }

        // Two tabs with a highlight that slides between them.
        Item {
            Layout.alignment: Qt.AlignVCenter
            Layout.leftMargin: 10
            implicitWidth: 368
            implicitHeight: 40
            Rectangle { anchors.fill: parent; radius: 20; color: Theme.surfaceHi
                Behavior on color { ColorAnimation { duration: Theme.normal } } }
            Rectangle {
                id: highlight
                y: 3; height: parent.height - 6; radius: 17
                width: 88
                x: 4 + 90 * Math.max(0, ["today", "health", "activity", "insights"].indexOf(root.tab))
                color: Theme.accent
                Behavior on x { NumberAnimation { duration: 320; easing.type: Easing.OutCubic } }
                Behavior on color { ColorAnimation { duration: Theme.normal } }
            }
            Row {
                anchors.fill: parent
                anchors.margins: 4
                Repeater {
                    model: [{ key: "today", label: "Today" }, { key: "health", label: "Health" }, { key: "activity", label: "Activity" }, { key: "insights", label: "Insights" }]
                    delegate: Item {
                        required property var modelData
                        width: 90; height: parent.height
                        Txt {
                            anchors.centerIn: parent
                            text: modelData.label; size: 14; strong: true
                            color: root.tab === modelData.key ? "white" : Theme.textDim
                        }
                        MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                            onClicked: root.tabSelected(modelData.key) }
                    }
                }
            }
        }

        Item { Layout.fillWidth: true }

        Row {
            visible: root.width > 1150
            spacing: 8
            Layout.alignment: Qt.AlignVCenter
            Repeater {
                model: ["#7C5CFF", "#3B82F6", "#14B8A6", "#F59E0B", "#F43F5E", "#A3E635"]
                delegate: Item {
                    required property string modelData
                    width: 26; height: 26
                    scale: swatchArea.containsMouse ? 1.18 : 1.0
                    Behavior on scale { NumberAnimation { duration: Theme.fast } }
                    Rectangle {
                        anchors.fill: parent; radius: 13; color: modelData
                        border.width: backend.accent.toLowerCase() === modelData.toLowerCase() ? 3 : 0
                        border.color: Theme.text
                    }
                    MouseArea { id: swatchArea; anchors.fill: parent; hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor; onClicked: backend.accent = modelData }
                }
            }
        }
        PillButton {
            Layout.alignment: Qt.AlignVCenter
            text: backend.darkMode ? "Light" : "Dark"
            onClicked: backend.darkMode = !backend.darkMode
        }
        PillButton {
            Layout.alignment: Qt.AlignVCenter
            primary: true; text: "Start a session"
            onClicked: root.startRequested()
        }
    }
}
