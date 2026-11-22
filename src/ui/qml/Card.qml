import QtQuick
import FocusUi

// A rounded surface with a soft shadow. It fades and slides in when created, and lifts slightly
// under the mouse. Anything declared inside a Card lands in its padded content area.
Item {
    id: root
    default property alias content: inner.data
    property int padding: 20
    property bool hoverable: true
    property int enterDelay: 0
    readonly property real lift: hoverable && hover.hovered ? 1.012 : 1.0

    opacity: 0
    scale: lift
    Behavior on scale { NumberAnimation { duration: Theme.normal; easing.type: Easing.OutCubic } }
    transform: Translate { id: slide; y: 22 }

    // Soft shadow: a few stacked translucent rounded rectangles, each a little bigger and lower.
    Repeater {
        model: 4
        Rectangle {
            required property int index
            anchors.fill: bg
            anchors.margins: -(index + 1) * 3
            anchors.topMargin: -(index + 1) * 3 + 5
            radius: bg.radius + (index + 1) * 3
            color: Theme.dark ? Qt.rgba(0, 0, 0, 0.10) : Qt.rgba(0.2, 0.25, 0.4, 0.022)
        }
    }

    Rectangle {
        id: bg
        anchors.fill: parent
        radius: Theme.radius
        color: Theme.surface
        border.width: 1
        border.color: root.hoverable && hover.hovered ? Qt.alpha(Theme.accent, 0.6) : Theme.border
        Behavior on color { ColorAnimation { duration: Theme.normal } }
        Behavior on border.color { ColorAnimation { duration: Theme.fast } }
    }

    Item {
        id: inner
        anchors.fill: parent
        anchors.margins: root.padding
    }

    HoverHandler { id: hover; enabled: root.hoverable }

    SequentialAnimation {
        running: true
        PauseAnimation { duration: root.enterDelay }
        ParallelAnimation {
            NumberAnimation { target: root; property: "opacity"; to: 1; duration: Theme.slow; easing.type: Easing.OutCubic }
            NumberAnimation { target: slide; property: "y"; to: 0; duration: Theme.slow; easing.type: Easing.OutCubic }
        }
    }
}
