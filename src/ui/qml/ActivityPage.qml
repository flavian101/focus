import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic
import FocusUi

// What your computer day looked like: time per hour by category, the apps you used, and how deep you worked.
Item {
    id: root
    property bool active: true

    opacity: active ? 1 : 0
    visible: opacity > 0.001
    Behavior on opacity { NumberAnimation { duration: Theme.normal; easing.type: Easing.OutCubic } }
    transform: Translate { y: root.active ? 0 : -24; Behavior on y { NumberAnimation { duration: 400; easing.type: Easing.OutCubic } } }
    onActiveChanged: if (active) backend.refreshActivity()

    readonly property var d: backend.activityDay
    // used by the self-test to show an opened row
    function expandApp(index) { var item = appList.itemAtIndex(index); if (item) item.open = true }
    property bool confirmDelete: false

    // 35070 -> "35k", 2204 -> "2.2k": the cards are narrow
    function compact(n) {
        n = n || 0
        return n >= 10000 ? Math.round(n / 1000) + "k" : n >= 1000 ? (n / 1000).toFixed(1) + "k" : n.toString()
    }
    function minutesText(v) {
        v = v || 0
        return v >= 60 ? Math.floor(v / 60) + "h " + Math.round(v % 60) + "m" : Math.round(v) + "m"
    }
    readonly property var order: ["focus", "ai", "neutral", "distraction", "away"]  // bottom to top in each bar
    readonly property var labels: ({ focus: "Focus", ai: "AI", neutral: "Other", distraction: "Distraction", away: "Away" })
    readonly property real maxHourMinutes: 60

    Timer { id: confirmTimer; interval: 3000; onTriggered: root.confirmDelete = false }

    ColumnLayout {
        anchors.fill: parent
        anchors.leftMargin: 36
        anchors.rightMargin: 36
        anchors.topMargin: 6
        anchors.bottomMargin: 24
        spacing: 18

        // ---- headline numbers ------------------------------------------------------------
        RowLayout {
            Layout.fillWidth: true
            spacing: 18
            StatCard {
                Layout.fillWidth: true; Layout.preferredHeight: 124
                title: "Active at the computer"; tint: Theme.cFocus
                value: root.d.activeMinutes !== undefined ? root.d.activeMinutes : 0
                formatter: function (v) { return root.minutesText(v) }
                sub: root.d.firstText ? "From " + root.d.firstText + " to " + root.d.lastText : "No activity yet today"
            }
            StatCard {
                Layout.fillWidth: true; Layout.preferredHeight: 124; enterDelay: 90
                title: "Longest focus block"; tint: Theme.accent
                value: root.d.longestFocus !== undefined ? root.d.longestFocus : 0
                formatter: function (v) { return root.minutesText(v) }
                sub: "Short glances do not break it"
            }
            StatCard {
                Layout.fillWidth: true; Layout.preferredHeight: 124; enterDelay: 180
                title: "App switches per hour"; tint: Theme.warn
                value: root.d.switchesPerHour !== undefined ? root.d.switchesPerHour : 0
                formatter: function (v) { return v.toFixed(1) }
                sub: (root.d.switches !== undefined ? root.d.switches : 0) + " switches today"
            }
            StatCard {
                Layout.fillWidth: true; Layout.preferredHeight: 124; enterDelay: 270
                title: "Time in AI tools"; tint: Theme.cAi
                value: root.d.aiMinutes !== undefined ? root.d.aiMinutes : 0
                formatter: function (v) { return root.minutesText(v) }
                sub: root.minutesText(root.d.distractionMinutes) + " in distractions"
            }
            StatCard {
                // How much of the focus time had real keyboard or mouse input: "VS Code was open" vs "I was working in it".
                Layout.fillWidth: true; Layout.preferredHeight: 124; enterDelay: 360
                title: "Hands-on focus"; tint: Theme.good
                value: root.d.handsOnFocusPercent !== undefined ? root.d.handsOnFocusPercent : 0
                formatter: function (v) { return root.d.hasEngagement ? Math.round(v) + "%" : "–" }
                sub: !root.d.hasEngagement ? "Not recorded for this day"
                     : root.d.hasInputCounts ? root.compact(root.d.keys) + " keys · " + root.compact(root.d.scrolls) + " scrolls"
                     : "of focus time had input"
            }
        }

        // ---- the day, hour by hour, and the apps ---------------------------------------------
        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 18

            Card {
                Layout.fillWidth: true; Layout.fillHeight: true; Layout.preferredWidth: 3
                hoverable: false; enterDelay: 360
                Txt { id: dayTitle; text: "Your day, hour by hour"; strong: true; size: 16 }

                Txt {
                    anchors.centerIn: parent
                    visible: !(root.d.activeMinutes > 0)
                    width: parent.width - 80; horizontalAlignment: Text.AlignHCenter; wrapMode: Text.Wrap
                    text: backend.trackingEnabled ? "Nothing recorded yet today. Keep Focus running in the tray and this fills in as you work."
                                                  : "Tracking is paused. Turn it on below to record your day."
                    dim: true; size: 14
                }

                Row {
                    id: hourRow
                    anchors.left: parent.left; anchors.right: parent.right
                    anchors.top: dayTitle.bottom; anchors.topMargin: 14
                    anchors.bottom: legend.top; anchors.bottomMargin: 10
                    spacing: 5
                    Repeater {
                        model: root.d.hours !== undefined ? root.d.hours : []
                        delegate: Item {
                            id: col
                            required property var modelData
                            required property int index
                            property bool ready: false
                            width: (hourRow.width - hourRow.spacing * 23) / 24
                            height: hourRow.height
                            Timer { interval: 250 + col.index * 35; running: true; onTriggered: col.ready = true }
                            readonly property real barArea: height - 20

                            // the bars are stacked: focus at the bottom, then AI, other, distraction, away
                            Column {
                                anchors.bottom: label.top; anchors.bottomMargin: 4
                                anchors.horizontalCenter: parent.horizontalCenter
                                width: parent.width
                                spacing: 0
                                Repeater {
                                    model: root.order.slice().reverse()   // Column stacks top-down, so list the top segment first
                                    delegate: Rectangle {
                                        required property string modelData
                                        width: parent.width
                                        radius: 3
                                        color: Theme.categoryColor(modelData === "ai" ? "ai" : modelData)
                                        height: col.ready ? Math.max(0, col.modelData[modelData] / root.maxHourMinutes * col.barArea) : 0
                                        Behavior on height { NumberAnimation { duration: 800; easing.type: Easing.OutCubic } }
                                    }
                                }
                            }
                            Txt {
                                id: label
                                anchors.horizontalCenter: parent.horizontalCenter
                                anchors.bottom: parent.bottom
                                text: col.index % 3 === 0 ? (col.index < 10 ? "0" + col.index : col.index) : ""
                                size: 11; dim: true
                            }
                        }
                    }
                }
                Row {
                    id: legend
                    anchors.bottom: parent.bottom
                    spacing: 16
                    Repeater {
                        model: root.order
                        delegate: Row {
                            required property string modelData
                            spacing: 6
                            Rectangle { anchors.verticalCenter: parent.verticalCenter; width: 10; height: 10; radius: 5
                                        color: Theme.categoryColor(modelData) }
                            Txt { text: root.labels[modelData]; dim: true; size: 12 }
                        }
                    }
                }
            }

            Card {
                Layout.fillWidth: true; Layout.fillHeight: true; Layout.preferredWidth: 2
                hoverable: false; enterDelay: 450
                Txt { id: appsTitle; text: "Where the time went"; strong: true; size: 16 }
                Txt {
                    anchors.right: parent.right; anchors.verticalCenter: appsTitle.verticalCenter
                    visible: appList.count > 0
                    text: "Click an app to correct it  ·  " + appList.count + (appList.count === 1 ? " app" : " apps"); dim: true; size: 12
                }

                // A scrolling list: with dozens of apps it never spills out of its card.
                ListView {
                    id: appList
                    anchors.top: appsTitle.bottom; anchors.topMargin: 12
                    anchors.bottom: parent.bottom
                    anchors.left: parent.left; anchors.right: parent.right
                    clip: true
                    spacing: 6
                    boundsBehavior: Flickable.StopAtBounds
                    model: root.d.apps !== undefined ? root.d.apps : []

                    ScrollBar.vertical: ScrollBar {
                        policy: ScrollBar.AsNeeded
                        implicitWidth: 8
                        contentItem: Rectangle {
                            implicitWidth: 6; radius: 3
                            color: Theme.textDim
                            opacity: parent.active || parent.pressed ? 0.8 : 0.35
                            Behavior on opacity { NumberAnimation { duration: Theme.normal } }
                        }
                        background: Item {}
                    }

                    delegate: Item {
                        id: appRow
                        required property var modelData
                        required property int index
                        property bool ready: false
                        property bool open: false
                        readonly property bool editable: !modelData.private
                        width: ListView.view.width - 14   // leave room for the scrollbar
                        height: open ? 84 : 38
                        Behavior on height { NumberAnimation { duration: 260; easing.type: Easing.OutCubic } }
                        clip: true
                        // rows grow in one after another, but never wait long: only the first dozen are staggered
                        Timer { interval: 300 + Math.min(appRow.index, 12) * 60; running: true; onTriggered: appRow.ready = true }
                        Rectangle { x: 0; y: 6; width: 10; height: 10; radius: 5
                                    color: Theme.categoryColor(appRow.modelData.category) }
                        Txt { x: 20; width: parent.width - 20 - mins.width - 8; text: appRow.modelData.app; size: 14; elide: Text.ElideRight }
                        Txt { id: mins; anchors.right: parent.right; text: root.minutesText(appRow.modelData.minutes); dim: true; size: 13 }
                        Txt { visible: appRow.modelData.rule !== ""; anchors.right: mins.left; anchors.rightMargin: 8
                              text: "corrected"; size: 11; color: Theme.accent }
                        MouseArea { x: 0; y: 0; width: parent.width; height: 38; enabled: appRow.editable
                                    cursorShape: Qt.PointingHandCursor; onClicked: appRow.open = !appRow.open }

                        // "This app is ..." choices, shown when the row is opened
                        Row {
                            x: 20; y: 46; spacing: 6; opacity: appRow.open ? 1 : 0
                            Behavior on opacity { NumberAnimation { duration: 200 } }
                            Repeater {
                                model: [{ key: "focus", label: "Focus" }, { key: "neutral", label: "Other" },
                                        { key: "distraction", label: "Distraction" }, { key: "ai", label: "AI" }]
                                delegate: Rectangle {
                                    id: pick
                                    required property var modelData
                                    readonly property bool current: appRow.modelData.category === modelData.key
                                    height: 26; width: pickText.implicitWidth + 22; radius: 13
                                    color: current ? Qt.alpha(Theme.categoryColor(modelData.key), 0.25) : "transparent"
                                    border.width: 1; border.color: current ? Theme.categoryColor(modelData.key) : Theme.border
                                    Txt { id: pickText; anchors.centerIn: parent; text: pick.modelData.label; size: 12; strong: true
                                          color: pick.current ? Theme.categoryColor(pick.modelData.key) : Theme.textDim }
                                    MouseArea { anchors.fill: parent; enabled: appRow.open; cursorShape: Qt.PointingHandCursor
                                                onClicked: backend.setAppCategory(appRow.modelData.name, pick.modelData.key) }
                                }
                            }
                            Rectangle {
                                visible: appRow.modelData.rule !== ""
                                height: 26; width: resetText.implicitWidth + 22; radius: 13
                                color: "transparent"; border.width: 1; border.color: Theme.border
                                Txt { id: resetText; anchors.centerIn: parent; text: "Reset"; size: 12; dim: true }
                                MouseArea { anchors.fill: parent; enabled: appRow.open; cursorShape: Qt.PointingHandCursor
                                            onClicked: backend.setAppCategory(appRow.modelData.name, "") }
                            }
                        }
                        Rectangle {
                            x: 20; y: 26; height: 4; radius: 2; color: Qt.alpha(Theme.categoryColor(appRow.modelData.category), 0.85)
                            width: appRow.ready ? (parent.width - 20) * appRow.modelData.share : 0
                            Behavior on width { NumberAnimation { duration: 700; easing.type: Easing.OutCubic } }
                        }
                    }
                }
            }
        }

        // ---- privacy and control ------------------------------------------------------------
        RowLayout {
            Layout.fillWidth: true
            spacing: 12
            Txt {
                Layout.fillWidth: true
                wrapMode: Text.Wrap
                text: (!backend.trackingEnabled ? "Tracking is paused. "
                       : backend.trackingMode === "detailed" ? "Tracking: full detail plus scroll, key and click counts. "
                       : backend.trackingMode === "work" ? "Tracking: full detail (app names). "
                       : backend.trackingMode === "private" ? "Tracking: category only right now, no app names. "
                       : "Tracking is off right now by your schedule. ")
                      + "Everything stays on this computer. Window titles and typed text are never stored."
                dim: true; size: 12
            }
            PillButton {
                text: backend.trackingEnabled ? "Pause tracking" : "Resume tracking"
                onClicked: backend.setTrackingEnabled(!backend.trackingEnabled)
            }
            PillButton {
                danger: true
                text: root.confirmDelete ? "Click again to delete" : "Delete today's data"
                onClicked: {
                    if (root.confirmDelete) { backend.clearTodayActivity(); root.confirmDelete = false }
                    else { root.confirmDelete = true; confirmTimer.restart() }
                }
            }
        }
    }
}
