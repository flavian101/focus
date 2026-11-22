import QtQuick
import QtQuick.Shapes
import FocusUi

// A ring that fills as `progress` goes from 0 to 1. Changes glide instead of jumping.
// Whatever you put inside the ring is centred in it.
Item {
    id: root
    default property alias content: center.data
    property real progress: 0
    property color ringColor: Theme.accent
    property real thickness: 18

    Behavior on progress { NumberAnimation { duration: 1000; easing.type: Easing.Linear } }
    Behavior on ringColor { ColorAnimation { duration: Theme.normal } }

    Shape {
        anchors.fill: parent
        // the faint full circle behind the progress
        ShapePath {
            strokeColor: Theme.surfaceHi
            strokeWidth: root.thickness
            fillColor: "transparent"
            PathAngleArc {
                centerX: root.width / 2; centerY: root.height / 2
                radiusX: (root.width - root.thickness) / 2; radiusY: (root.height - root.thickness) / 2
                startAngle: -90; sweepAngle: 359.9
            }
        }
        // a wide, faint copy of the arc underneath gives it a glow
        ShapePath {
            strokeColor: root.progress > 0.003 ? Qt.alpha(root.ringColor, 0.20) : "transparent"
            strokeWidth: root.thickness + 12
            fillColor: "transparent"
            capStyle: ShapePath.RoundCap
            PathAngleArc {
                centerX: root.width / 2; centerY: root.height / 2
                radiusX: (root.width - root.thickness) / 2; radiusY: (root.height - root.thickness) / 2
                startAngle: -90; sweepAngle: 360 * root.progress
            }
        }
        // the progress arc itself
        ShapePath {
            strokeColor: root.progress > 0.003 ? root.ringColor : "transparent"
            strokeWidth: root.thickness
            fillColor: "transparent"
            capStyle: ShapePath.RoundCap
            PathAngleArc {
                centerX: root.width / 2; centerY: root.height / 2
                radiusX: (root.width - root.thickness) / 2; radiusY: (root.height - root.thickness) / 2
                startAngle: -90; sweepAngle: 360 * root.progress
            }
        }
    }
    Item { id: center; anchors.fill: parent }
}
