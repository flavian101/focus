pragma Singleton
import QtQuick

// One place for every colour, size and timing. Change a value here and the whole app follows.
// Main.qml binds `dark` and `accent` to the saved settings.
QtObject {
    property bool dark: true
    property color accent: "#7C5CFF"

    readonly property color bg: dark ? "#0E1016" : "#F3F4F9"
    readonly property color surface: dark ? "#161923" : "#FFFFFF"
    readonly property color surfaceHi: dark ? "#1F2333" : "#EDEFF6"
    readonly property color border: dark ? "#2A2F42" : "#DCDFEA"
    readonly property color text: dark ? "#ECEEF6" : "#171A24"
    readonly property color textDim: dark ? "#8B92AA" : "#667089"

    readonly property color good: "#34D399"
    readonly property color warn: "#F5B84B"
    readonly property color bad: "#F87171"

    // One colour per kind of time, used by the breakdown bar and legend.
    readonly property color cFocus: "#34D399"
    readonly property color cAi: "#38BDF8"
    readonly property color cNeutral: dark ? "#64748B" : "#94A3B8"
    readonly property color cDistraction: "#F87171"
    readonly property color cAway: dark ? "#3B4256" : "#CBD2E0"

    readonly property int radius: 18
    readonly property int fast: 140
    readonly property int normal: 280
    readonly property int slow: 700
    readonly property string fontFamily: "Segoe UI"

    function moodColor(v) {
        var colors = ["#F87171", "#FB923C", "#FACC15", "#A3E635", "#34D399"]
        return colors[Math.max(1, Math.min(5, Math.round(v))) - 1]
    }

    function priorityColor(p) {  // 1 = Must, 2 = Should, 3 = Could
        if (p === 1) return accent
        if (p === 2) return cAi
        return cNeutral
    }

    function categoryColor(key) {
        if (key === "focus") return cFocus
        if (key === "ai") return cAi
        if (key === "distraction") return cDistraction
        if (key === "away") return cAway
        return cNeutral
    }
}
