import QtQuick
import QtQuick.Layouts
import FocusUi

Item {
    id: root
    property bool active: true
    opacity: active ? 1 : 0
    visible: opacity > 0.001
    Behavior on opacity { NumberAnimation { duration: Theme.normal; easing.type: Easing.OutCubic } }
    transform: Translate { y: root.active ? 0 : -24; Behavior on y { NumberAnimation { duration: 400; easing.type: Easing.OutCubic } } }

    readonly property var st: backend.stats

    function minutesText(v) {
        return v >= 60 ? Math.floor(v / 60) + "h " + Math.round(v % 60) + "m" : Math.round(v) + "m"
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.leftMargin: 36
        anchors.rightMargin: 36
        anchors.topMargin: 6
        anchors.bottomMargin: 36
        spacing: 22

        // ---- headline numbers ------------------------------------------------------------
        RowLayout {
            Layout.fillWidth: true
            spacing: 18
            StatCard {
                Layout.fillWidth: true; Layout.preferredHeight: 124; enterDelay: 0
                title: "Focus time"; tint: Theme.cFocus
                value: root.st.focusMinutes !== undefined ? root.st.focusMinutes : 0
                formatter: function (v) { return root.minutesText(v) }
                sub: root.st.awayMinutes !== undefined ? Math.round(root.st.awayMinutes) + " min away from keyboard" : ""
            }
            StatCard {
                Layout.fillWidth: true; Layout.preferredHeight: 124; enterDelay: 90
                title: "Sessions completed"; tint: Theme.accent
                value: root.st.completed !== undefined ? root.st.completed : 0
                sub: (root.st.abandoned !== undefined ? root.st.abandoned : 0) + " ended early"
            }
            StatCard {
                Layout.fillWidth: true; Layout.preferredHeight: 124; enterDelay: 180
                title: "AI visits per session"; tint: Theme.cAi
                value: root.st.aiPerSession !== undefined ? root.st.aiPerSession : 0
                formatter: function (v) { return v.toFixed(1) }
                sub: root.st.aiSharePct !== undefined
                     ? Math.round(root.st.aiSharePct) + "% of your time · " + root.st.overBudget + " over budget" : ""
            }
            StatCard {
                Layout.fillWidth: true; Layout.preferredHeight: 124; enterDelay: 270
                title: "Mood change in a session"
                tint: Theme.moodColor(3 + (root.st.moodChange !== undefined ? root.st.moodChange : 0))
                value: root.st.moodChange !== undefined ? root.st.moodChange : 0
                formatter: function (v) { return (v >= 0 ? "+" : "") + v.toFixed(1) }
                sub: root.st.hasMood ? root.st.moodBefore.toFixed(1) + " → " + root.st.moodAfter.toFixed(1) + " average mood"
                                     : "Check in before and after a session"
            }
        }

        // ---- chart and history -----------------------------------------------------------
        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 18

            WeekChart {
                Layout.fillWidth: true; Layout.fillHeight: true; Layout.preferredWidth: 3
                enterDelay: 360
                hoverable: false
                days: backend.week
            }

            Card {
                Layout.fillWidth: true; Layout.fillHeight: true; Layout.preferredWidth: 2
                enterDelay: 450
                hoverable: false

                Txt { id: recentTitle; text: "Recent sessions"; strong: true; size: 16 }
                Txt {
                    visible: backend.recent.length === 0
                    anchors.top: recentTitle.bottom; anchors.topMargin: 16
                    width: parent.width; wrapMode: Text.Wrap
                    text: "No sessions yet. Start one to see your trends here."; dim: true; size: 14
                }
                Column {
                    anchors.top: recentTitle.bottom
                    anchors.topMargin: 12
                    width: parent.width
                    spacing: 2
                    Repeater {
                        model: backend.recent
                        delegate: Item {
                            id: row
                            required property var modelData
                            width: parent.width
                            height: 46
                            Rectangle {
                                x: 0; anchors.verticalCenter: parent.verticalCenter
                                width: 10; height: 10; radius: 5
                                color: row.modelData.completed ? Theme.good : Theme.cNeutral
                            }
                            Column {
                                anchors.verticalCenter: parent.verticalCenter
                                x: 22
                                width: parent.width - 22 - aiChip.width - 8
                                spacing: 1
                                Txt { width: parent.width; text: row.modelData.goal; size: 14; elide: Text.ElideRight }
                                Txt { text: row.modelData.when + " · " + row.modelData.minutes + " min"; dim: true; size: 12 }
                            }
                            Rectangle {
                                id: aiChip
                                anchors.right: parent.right
                                anchors.verticalCenter: parent.verticalCenter
                                width: aiText.implicitWidth + 18; height: 24; radius: 12
                                color: "transparent"
                                border.width: 1
                                border.color: row.modelData.overBudget ? Theme.bad : Theme.cAi
                                Txt { id: aiText; anchors.centerIn: parent; text: "AI " + row.modelData.aiVisits; size: 11
                                      color: row.modelData.overBudget ? Theme.bad : Theme.cAi }
                            }
                        }
                    }
                }
            }
        }
    }
}
