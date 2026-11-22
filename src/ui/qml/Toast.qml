import QtQuick
import FocusUi

// A short message that slides up from the bottom, then leaves by itself.
Item {
    id: root
    property bool shown: false
    function show(title, body) {
        titleText.text = title
        bodyText.text = body
        shown = true
        hideTimer.restart()
    }

    // Bottom-left: the controls and the headline numbers live on the right and top.
    width: Math.min(420, parent.width - 72)
    height: column.implicitHeight + 30
    x: 36
    y: shown ? parent.height - height - 28 : parent.height + 12
    opacity: shown ? 1 : 0
    Behavior on y { NumberAnimation { duration: 450; easing.type: Easing.OutBack } }
    Behavior on opacity { NumberAnimation { duration: Theme.normal } }

    Timer { id: hideTimer; interval: 5000; onTriggered: root.shown = false }

    Rectangle {
        anchors.fill: parent
        radius: 18
        color: "#232839"
        border.width: 1
        border.color: Qt.alpha(Theme.accent, 0.7)
    }
    Column {
        id: column
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        anchors.margins: 20
        spacing: 4
        Txt { id: titleText; width: parent.width; strong: true; size: 15; color: "white" }
        Txt { id: bodyText; width: parent.width; size: 13; color: "#B4BAD0"; wrapMode: Text.Wrap }
    }
}
