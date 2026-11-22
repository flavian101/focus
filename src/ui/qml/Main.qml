import QtQuick
import QtQuick.Controls.Basic
import FocusUi

ApplicationWindow {
    id: win
    width: 1080
    height: 720
    minimumWidth: 900
    minimumHeight: 660
    visible: true
    title: "Focus"
    color: Theme.bg
    Behavior on color { ColorAnimation { duration: Theme.normal } }

    // The saved settings drive the theme.
    Binding { target: Theme; property: "dark"; value: backend.darkMode }
    Binding { target: Theme; property: "accent"; value: backend.accent }

    function openStartSheet() { startSheet.open = true }

    DashboardPage {
        anchors.fill: parent
        active: backend.phase === "idle"
        onStartRequested: win.openStartSheet()
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
