import QtQuick
import QtQuick.Layouts
import FocusUi

Item {
    id: root
    property bool active: false

    opacity: active ? 1 : 0
    visible: opacity > 0.001
    Behavior on opacity { NumberAnimation { duration: Theme.normal; easing.type: Easing.OutCubic } }
    transform: Translate { y: root.active ? 0 : 24; Behavior on y { NumberAnimation { duration: 400; easing.type: Easing.OutCubic } } }

    readonly property bool paused: backend.phase === "paused"
    readonly property bool overBudget: backend.aiBudget >= 0 && backend.aiUsed > backend.aiBudget
    property int seenAi: 0
    readonly property var cats: backend.categories
    readonly property real totalSeconds: {
        var t = 0
        for (var i = 0; i < cats.length; i++) t += cats[i].seconds
        return t
    }
    function minutesText(sec) { return Math.round(sec / 60) + "m" }

    // A quick "bump" of the ring each time an AI visit is counted.
    SequentialAnimation {
        id: bump
        NumberAnimation { target: ring; property: "scale"; to: 1.045; duration: 140; easing.type: Easing.OutCubic }
        NumberAnimation { target: ring; property: "scale"; to: 1.0; duration: 420; easing.type: Easing.OutBack }
    }
    Connections {
        target: backend
        function onChanged() {
            if (backend.aiUsed > root.seenAi) bump.restart()
            root.seenAi = backend.aiUsed
        }
    }

    RowLayout {
        anchors.fill: parent
        anchors.margins: 36
        spacing: 36

        // ---- the ring --------------------------------------------------------------------
        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            Column {
                anchors.centerIn: parent
                spacing: 22
                width: parent.width

                ProgressRing {
                    id: ring
                    anchors.horizontalCenter: parent.horizontalCenter
                    width: Math.min(340, root.height - 220); height: width
                    progress: backend.progress
                    ringColor: root.paused ? Theme.warn : (!backend.present ? Theme.textDim : (root.overBudget ? Theme.bad : Theme.accent))

                    Column {
                        anchors.centerIn: parent
                        spacing: 4
                        Txt {
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: backend.remainingText
                            size: Math.round(ring.width * 0.2); strong: true
                        }
                        Txt {
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: root.paused ? "Paused" : (!backend.present ? "Away from keyboard" : "Focusing")
                            dim: true; size: 15
                        }
                    }
                }

                Txt {
                    anchors.horizontalCenter: parent.horizontalCenter
                    width: Math.min(parent.width, 460)
                    horizontalAlignment: Text.AlignHCenter
                    text: backend.goal; size: 22; strong: true; wrapMode: Text.Wrap; maximumLineCount: 2; elide: Text.ElideRight
                }
                Rectangle {
                    anchors.horizontalCenter: parent.horizontalCenter
                    width: Math.min(parent.width, firstTry.implicitWidth + 28); height: 32; radius: 16
                    color: Theme.surfaceHi
                    visible: backend.approach !== ""
                    Txt { id: firstTry; anchors.centerIn: parent; width: parent.width - 28; elide: Text.ElideRight
                          text: "First try: " + backend.approach; dim: true; size: 13; horizontalAlignment: Text.AlignHCenter }
                }
            }
        }

        // ---- right column ----------------------------------------------------------------
        ColumnLayout {
            Layout.preferredWidth: 380
            Layout.maximumWidth: 380
            Layout.fillHeight: true
            spacing: 18

            Card {
                Layout.fillWidth: true; Layout.preferredHeight: 178
                hoverable: false
                Txt { id: breakdownTitle; text: "Where your time went"; strong: true; size: 16 }

                Rectangle {
                    id: track
                    anchors.top: breakdownTitle.bottom; anchors.topMargin: 14
                    width: parent.width; height: 14; radius: 7; color: Theme.surfaceHi; clip: true
                    Row {
                        anchors.fill: parent
                        Repeater {
                            model: root.cats
                            delegate: Rectangle {
                                required property var modelData
                                height: track.height
                                width: root.totalSeconds > 0 ? modelData.seconds / root.totalSeconds * track.width : 0
                                color: Theme.categoryColor(modelData.key)
                                Behavior on width { NumberAnimation { duration: 700; easing.type: Easing.OutCubic } }
                            }
                        }
                    }
                }
                Grid {
                    anchors.top: track.bottom; anchors.topMargin: 16
                    width: parent.width
                    columns: 2; columnSpacing: 8; rowSpacing: 10
                    Repeater {
                        model: root.cats
                        delegate: Row {
                            required property var modelData
                            width: (parent.width - 8) / 2
                            spacing: 8
                            Rectangle { anchors.verticalCenter: parent.verticalCenter; width: 10; height: 10; radius: 5
                                        color: Theme.categoryColor(modelData.key) }
                            Txt { text: modelData.label; dim: true; size: 13 }
                            Txt { text: root.minutesText(modelData.seconds); size: 13; strong: true }
                        }
                    }
                }
            }

            Card {
                Layout.fillWidth: true; Layout.preferredHeight: 132
                hoverable: false
                enterDelay: 100
                Txt { id: aiTitle; text: "AI budget"; strong: true; size: 16 }
                Txt {
                    anchors.right: parent.right
                    text: backend.aiBudget < 0 ? backend.aiUsed + " visits, no limit set"
                                               : backend.aiUsed + " of " + backend.aiBudget + " used"
                    color: root.overBudget ? Theme.bad : Theme.textDim; size: 13
                }
                Row {
                    id: pips
                    anchors.top: aiTitle.bottom; anchors.topMargin: 16
                    spacing: 8
                    Repeater {
                        model: backend.aiBudget < 0 ? 0 : Math.min(backend.aiBudget, 12)
                        delegate: Rectangle {
                            required property int index
                            readonly property bool used: index < backend.aiUsed
                            width: 22; height: 22; radius: 11
                            color: used ? (root.overBudget ? Theme.bad : Theme.cAi) : "transparent"
                            border.width: 2
                            border.color: used ? "transparent" : Theme.border
                            scale: used ? 1.0 : 0.88
                            Behavior on scale { NumberAnimation { duration: 300; easing.type: Easing.OutBack } }
                            Behavior on color { ColorAnimation { duration: Theme.normal } }
                        }
                    }
                }
                Txt {
                    anchors.top: pips.bottom; anchors.topMargin: 12
                    width: parent.width; wrapMode: Text.Wrap
                    text: root.overBudget ? "Over budget. Write down what you've tried. Is this a real blocker?"
                                          : "Visits are counted when you switch to an AI window."
                    dim: !root.overBudget; color: root.overBudget ? Theme.bad : Theme.textDim; size: 12
                }
            }

            Item { Layout.fillHeight: true }

            RowLayout {
                Layout.fillWidth: true
                spacing: 10
                PillButton {
                    Layout.fillWidth: true
                    primary: true
                    text: root.paused ? "Resume" : "Pause"
                    onClicked: root.paused ? backend.resume() : backend.pause()
                }
                PillButton { Layout.fillWidth: true; text: "Log AI use"; onClicked: backend.logAiUse() }
                PillButton { Layout.fillWidth: true; text: "End"; danger: true; onClicked: backend.abandon() }
            }
        }
    }
}
