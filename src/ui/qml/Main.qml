import QtQuick
import QtQuick.Controls.Basic
import FocusUi

ApplicationWindow {
    id: win
    width: 1200
    height: 720
    minimumWidth: 980
    minimumHeight: 660
    visible: true
    title: "Focus"
    color: Theme.bg
    Behavior on color { ColorAnimation { duration: Theme.normal } }

    property string tab: "today"  // "today" | "health" | "activity" | "insights"

    // The saved settings drive the theme.
    Binding { target: Theme; property: "dark"; value: backend.darkMode }
    Binding { target: Theme; property: "accent"; value: backend.accent }

    function openStartSheet() { startSheet.reset(); startSheet.open = true }
    function openStartSheetFor(taskId, title, minutes) { startSheet.openFor(taskId, title, minutes) }

    Header {
        id: header
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        tab: win.tab
        opacity: backend.phase === "idle" ? 1 : 0
        visible: opacity > 0.001
        Behavior on opacity { NumberAnimation { duration: Theme.normal } }
        onTabSelected: (name) => win.tab = name
        onStartRequested: win.openStartSheet()
    }

    Item {
        anchors.top: header.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom

        TodayPage {
            anchors.fill: parent
            active: backend.phase === "idle" && win.tab === "today"
            onFocusTask: (taskId, title, minutes) => win.openStartSheetFor(taskId, title, minutes)
        }
        HealthPage {
            anchors.fill: parent
            active: backend.phase === "idle" && win.tab === "health"
        }
        ActivityPage {
            anchors.fill: parent
            active: backend.phase === "idle" && win.tab === "activity"
        }
        InsightsPage {
            anchors.fill: parent
            active: backend.phase === "idle" && win.tab === "insights"
        }
    }

    SessionPage {
        anchors.fill: parent
        active: backend.phase !== "idle"
    }

    StartSheet { id: startSheet }
    CheckInSheet { }
    Toast { id: toast }

    Connections {
        target: backend
        function onNotification(title, body) { toast.show(title, body) }
        function onErrorOccurred(message) { toast.show("Something went wrong", message) }
    }
}
