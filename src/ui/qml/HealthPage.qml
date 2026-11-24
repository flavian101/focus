import QtQuick
import QtQuick.Layouts
import FocusUi

// Water on pace, and three meals you can answer: ate, skipped, or still to come.
Item {
    id: root
    property bool active: true

    opacity: active ? 1 : 0
    visible: opacity > 0.001
    Behavior on opacity { NumberAnimation { duration: Theme.normal; easing.type: Easing.OutCubic } }
    transform: Translate { y: root.active ? 0 : -24; Behavior on y { NumberAnimation { duration: 400; easing.type: Easing.OutCubic } } }

    readonly property var w: backend.wellness
    readonly property int glasses: w.glasses !== undefined ? w.glasses : 0
    readonly property int goal: w.goal !== undefined ? w.goal : 8
    readonly property int expected: w.expected !== undefined ? w.expected : 0

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 36
        anchors.rightMargin: 36
        anchors.topMargin: 6
        anchors.bottomMargin: 36
        spacing: 18

        // ================================================================== water
        Card {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.preferredWidth: 2
            hoverable: false
            padding: 22

            Txt { id: waterTitle; text: "Water"; size: 20; strong: true }
            Row {
                anchors.right: parent.right
                anchors.verticalCenter: waterTitle.verticalCenter
                spacing: 6
                Txt { anchors.verticalCenter: parent.verticalCenter; text: "Goal"; dim: true; size: 13 }
                PillButton { implicitHeight: 30; text: "−"; enabled: root.goal > 1; onClicked: backend.setWaterGoal(root.goal - 1) }
                Txt { anchors.verticalCenter: parent.verticalCenter; width: 22; horizontalAlignment: Text.AlignHCenter
                      text: root.goal; strong: true; size: 14 }
                PillButton { implicitHeight: 30; text: "+"; enabled: root.goal < 20; onClicked: backend.setWaterGoal(root.goal + 1) }
            }

            ProgressRing {
                id: ring
                anchors.top: waterTitle.bottom
                anchors.topMargin: 24
                anchors.horizontalCenter: parent.horizontalCenter
                width: 216; height: 216; thickness: 16
                progress: Math.min(1, root.glasses / Math.max(1, root.goal))
                ringColor: Theme.cAi
                Column {
                    anchors.centerIn: parent
                    spacing: 0
                    Txt { anchors.horizontalCenter: parent.horizontalCenter; text: root.glasses; size: 58; strong: true }
                    Txt { anchors.horizontalCenter: parent.horizontalCenter; text: "of " + root.goal + " glasses"; dim: true; size: 14 }
                }
            }

            // Pace: the filled part is what you drank, the tick is where you should be by now.
            Item {
                id: pace
                anchors.top: ring.bottom
                anchors.topMargin: 26
                width: parent.width; height: 34
                Rectangle {
                    y: 14; width: parent.width; height: 8; radius: 4; color: Theme.surfaceHi
                    Rectangle {
                        height: parent.height; radius: 4; color: Theme.cAi
                        width: Math.min(1, root.glasses / Math.max(1, root.goal)) * parent.width
                        Behavior on width { NumberAnimation { duration: 700; easing.type: Easing.OutCubic } }
                    }
                }
                Rectangle {
                    y: 8; width: 3; height: 20; radius: 1.5; color: Theme.text
                    x: Math.min(1, root.expected / Math.max(1, root.goal)) * (parent.width - 3)
                    Behavior on x { NumberAnimation { duration: 700; easing.type: Easing.OutCubic } }
                }
                Txt { y: 30; text: "pace"; dim: true; size: 11
                      x: Math.max(0, Math.min(parent.width - width, Math.min(1, root.expected / Math.max(1, root.goal)) * parent.width - width / 2)) }
            }
            Txt {
                id: paceText
                anchors.top: pace.bottom
                anchors.topMargin: 18
                width: parent.width; wrapMode: Text.Wrap
                size: 14
                color: root.glasses >= root.goal ? Theme.good : (root.w.onPace ? Theme.good : Theme.warn)
                text: root.glasses >= root.goal ? "Goal reached. Nice."
                      : (root.w.onPace ? "On pace. Keep it up."
                                       : (root.w.behind || 0) + ((root.w.behind || 0) === 1 ? " glass" : " glasses") + " behind pace. A glass now catches you up.")
            }

            Row {
                anchors.top: paceText.bottom
                anchors.topMargin: 22
                spacing: 10
                PillButton { primary: true; tint: Theme.cAi; text: "Add a glass"; implicitWidth: 170; onClicked: backend.logWater() }
                PillButton { text: "Undo"; enabled: root.glasses > 0; onClicked: backend.undoWater() }
            }
            Txt {
                anchors.bottom: parent.bottom
                width: parent.width; wrapMode: Text.Wrap
                text: "Reminders only come when you fall behind pace, between " + (root.w.startText || "08:00") + " and " + (root.w.endText || "22:00") + "."
                dim: true; size: 12
            }
        }

        // ================================================================== meals
        Card {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.preferredWidth: 3
            hoverable: false
            enterDelay: 120
            padding: 22

            Txt { id: mealsTitle; text: "Meals"; size: 20; strong: true }

            Column {
                anchors.top: mealsTitle.bottom
                anchors.topMargin: 18
                width: parent.width
                spacing: 12

                Repeater {
                    model: root.w.meals !== undefined ? root.w.meals : []
                    delegate: Item {
                        id: meal
                        required property var modelData
                        readonly property string state: modelData.state
                        readonly property bool answered: state === "ate" || state === "skipped"
                        width: parent.width
                        height: 88

                        Rectangle {
                            id: card
                            anchors.fill: parent
                            radius: 18
                            color: Theme.surfaceHi
                            border.width: 1
                            border.color: meal.state === "due" ? Theme.accent : Theme.border
                            Behavior on border.color { ColorAnimation { duration: Theme.normal } }
                        }
                        // A soft pulse around the meal that is due right now.
                        Rectangle {
                            anchors.fill: card
                            radius: 18; color: "transparent"
                            border.width: 2; border.color: Theme.accent
                            opacity: 0
                            SequentialAnimation on opacity {
                                loops: Animation.Infinite
                                running: meal.state === "due" && root.active
                                NumberAnimation { to: 0.7; duration: 900; easing.type: Easing.InOutSine }
                                NumberAnimation { to: 0.0; duration: 900; easing.type: Easing.InOutSine }
                            }
                        }

                        Column {
                            x: 20; anchors.verticalCenter: parent.verticalCenter
                            spacing: 4
                            Txt { text: meal.modelData.name; size: 18; strong: true
                                  opacity: meal.answered ? 0.6 : 1; Behavior on opacity { NumberAnimation { duration: 250 } } }
                            Row {
                                spacing: 10
                                Txt { text: meal.modelData.window; dim: true; size: 13; anchors.verticalCenter: parent.verticalCenter }
                                Rectangle {
                                    height: 22; width: stateText.implicitWidth + 20; radius: 11
                                    color: "transparent"; border.width: 1
                                    border.color: meal.state === "ate" ? Theme.good : meal.state === "due" ? Theme.accent
                                                : meal.state === "missed" ? Theme.warn : Theme.border
                                    Txt {
                                        id: stateText; anchors.centerIn: parent; size: 11; strong: true
                                        text: meal.state === "ate" ? "Eaten" : meal.state === "due" ? "Due now" : meal.state === "missed" ? "Missed"
                                              : meal.state === "skipped" ? "Skipped" : "Later today"
                                        color: meal.state === "ate" ? Theme.good : meal.state === "due" ? Theme.accent
                                             : meal.state === "missed" ? Theme.warn : Theme.textDim
                                    }
                                }
                            }
                        }

                        Row {
                            anchors.right: parent.right; anchors.rightMargin: 16
                            anchors.verticalCenter: parent.verticalCenter
                            spacing: 8
                            PillButton {
                                visible: !meal.answered
                                implicitHeight: 36
                                primary: meal.state === "due"
                                text: "I ate"
                                onClicked: backend.setMeal(meal.modelData.id, "ate")
                            }
                            PillButton {
                                visible: !meal.answered && meal.state !== "upcoming"
                                implicitHeight: 36
                                text: "Skip"
                                onClicked: backend.setMeal(meal.modelData.id, "skipped")
                            }
                            PillButton {
                                visible: meal.answered
                                implicitHeight: 36
                                text: "Undo"
                                onClicked: backend.setMeal(meal.modelData.id, "clear")
                            }
                        }
                    }
                }
            }

            Txt {
                anchors.bottom: parent.bottom
                width: parent.width; wrapMode: Text.Wrap
                text: "Focus reminds you while it runs in the tray, even with this window closed. Meal times use the defaults for now."
                dim: true; size: 12
            }
        }
    }
}
