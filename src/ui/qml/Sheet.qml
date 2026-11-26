import QtQuick
import QtQuick.Controls.Basic
import FocusUi

// A modal panel that rises over a dimmed backdrop. Put content inside it; it sizes itself to fit.
Item {
    id: root
    property bool open: false
    property int panelWidth: 540
    default property alias content: panel.data
    signal dismissed()

    anchors.fill: parent
    opacity: open ? 1 : 0
    visible: opacity > 0.001
    Behavior on opacity { NumberAnimation { duration: Theme.normal; easing.type: Easing.OutCubic } }

    Rectangle {
        anchors.fill: parent
        color: "#000000"
        opacity: 0.55
        MouseArea { anchors.fill: parent; onClicked: root.dismissed() }
    }

    Rectangle {
        id: sheet
        width: Math.min(root.panelWidth, root.width - 48)
        height: Math.min(panel.childrenRect.height + 56, root.height - 32)
        anchors.horizontalCenter: parent.horizontalCenter
        y: (root.height - height) / 2 + (root.open ? 0 : 48)
        Behavior on y { NumberAnimation { duration: 420; easing.type: Easing.OutCubic } }
        radius: 26
        color: Theme.surface
        border.width: 1
        border.color: Theme.border

        MouseArea { anchors.fill: parent }  // clicks on the panel must not reach the backdrop
        // The content scrolls if the sheet is taller than the window allows.
        Flickable {
            id: flick
            x: 28
            y: 28
            width: parent.width - 56
            height: parent.height - 56
            contentWidth: width
            contentHeight: panel.childrenRect.height
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: ScrollBar {
                policy: ScrollBar.AsNeeded
                implicitWidth: 8
                contentItem: Rectangle { implicitWidth: 6; radius: 3; color: Theme.textDim; opacity: 0.5 }
                background: Item {}
            }
            Item {
                id: panel
                width: flick.width - 14   // room for the scrollbar, so it never covers the content
                height: childrenRect.height
            }
        }
    }
}
