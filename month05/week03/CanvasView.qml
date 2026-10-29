pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls

Rectangle {
    id: canvas

    required property var editor
    property real panX: 72
    property real panY: height - 110
    property real zoom: 1

    function fitView() {
        const bounds = canvas.editor.geometryBounds();
        const left = bounds.left, bottom = bounds.bottom, right = bounds.right, top = bounds.top;
        zoom = Math.max(0.05, Math.min(10, Math.min((width - 130) / (right - left), (height - 160) / (top - bottom))));
        panX = (width - (right - left) * zoom) / 2 - left * zoom;
        panY = (height + (top - bottom) * zoom) / 2 + bottom * zoom;
    }
    function zoomAt(px, py, factor) {
        const next = Math.max(0.05, Math.min(20, zoom * factor));
        const ratio = next / zoom;
        panX = px - (px - panX) * ratio;
        panY = py - (py - panY) * ratio;
        zoom = next;
    }

    border.color: "#2d4054"
    clip: true
    color: "#101e2b"
    focus: true
    radius: 8

    Keys.onEscapePressed: canvas.editor.cancelDrag()

    Repeater {
        model: Math.ceil(canvas.width / 40) + 1

        Rectangle {
            required property int index

            color: "#1d3040"
            height: canvas.height
            width: 1
            x: index * 40 + canvas.panX % 40
        }
    }
    Repeater {
        model: Math.ceil(canvas.height / 40) + 1

        Rectangle {
            required property int index

            color: "#1d3040"
            height: 1
            width: canvas.width
            y: index * 40 + canvas.panY % 40
        }
    }
    Rectangle {
        color: "#3c6179"
        height: canvas.height
        width: 1
        x: canvas.panX
    }
    Rectangle {
        color: "#3c6179"
        height: 1
        width: canvas.width
        y: canvas.panY
    }
    MouseArea {
        property real lastX: 0
        property real lastY: 0

        acceptedButtons: Qt.LeftButton | Qt.MiddleButton
        anchors.fill: parent

        onPositionChanged: mouse => {
            if ((pressedButtons & Qt.MiddleButton) && canvas.editor.courseWeek >= 4) {
                canvas.panX += mouse.x - lastX;
                canvas.panY += mouse.y - lastY;
            }
            lastX = mouse.x;
            lastY = mouse.y;
        }
        onPressed: mouse => {
            lastX = mouse.x;
            lastY = mouse.y;
            if (mouse.button === Qt.LeftButton && canvas.editor.courseWeek >= 2)
                canvas.editor.select("");
        }
        onWheel: wheel => {
            if (canvas.editor.courseWeek >= 4)
                canvas.zoomAt(wheel.x, wheel.y, Math.pow(1.0015, wheel.angleDelta.y));
            wheel.accepted = true;
        }
    }
    Repeater {
        id: shapes

        model: canvas.editor

        delegate: Rectangle {
            id: shape

            readonly property real displayX: previewing ? canvas.editor.previewX : modelX
            readonly property real displayY: previewing ? canvas.editor.previewY : modelY
            required property bool draggable
            required property bool hasGeometry
            required property real modelLength
            required property real modelWidth
            required property real modelX
            required property real modelY
            required property string objectId
            required property string objectNameText
            required property bool objectVisible
            readonly property bool previewing: canvas.editor.previewId === objectId
            required property bool stale

            border.color: stale ? "#ffc078" : canvas.editor.selectedId === objectId ? "#97e5fc" : "#55b5d9"
            border.width: canvas.editor.selectedId === objectId ? 3 : 1
            color: stale ? "#554337" : "#204a61"
            height: modelWidth * canvas.zoom
            objectName: "rectangle-" + objectId
            radius: 2
            visible: objectVisible && hasGeometry
            width: modelLength * canvas.zoom
            x: canvas.panX + displayX * canvas.zoom
            y: canvas.panY - (displayY + modelWidth) * canvas.zoom

            Label {
                anchors.centerIn: parent
                color: shape.stale ? "#ffd39d" : "#d5eff9"
                horizontalAlignment: Text.AlignHCenter
                text: shape.objectNameText + "  #" + shape.objectId + (shape.stale ? "\n上一次有效图形" : "")
                visible: parent.width > 70 && parent.height > 35
            }
            MouseArea {
                property point startPoint
                property real startX: 0
                property real startY: 0

                acceptedButtons: Qt.LeftButton
                anchors.fill: parent
                enabled: canvas.editor.courseWeek >= 2

                onCanceled: canvas.editor.cancelDrag()
                onPositionChanged: mouse => {
                    if (pressed && shape.previewing) {
                        const point = mapToItem(canvas, mouse.x, mouse.y);
                        canvas.editor.previewDrag(startX + (point.x - startPoint.x) / canvas.zoom, startY - (point.y - startPoint.y) / canvas.zoom);
                    }
                }
                onPressed: mouse => {
                    canvas.forceActiveFocus();
                    canvas.editor.select(shape.objectId);
                    startPoint = mapToItem(canvas, mouse.x, mouse.y);
                    startX = shape.modelX;
                    startY = shape.modelY;
                    if (canvas.editor.courseWeek >= 4)
                        canvas.editor.beginDrag(shape.objectId);
                }
                onReleased: {
                    if (shape.previewing)
                        canvas.editor.finishDrag();
                }
                onWheel: wheel => {
                    if (canvas.editor.courseWeek >= 4) {
                        const point = mapToItem(canvas, wheel.x, wheel.y);
                        canvas.zoomAt(point.x, point.y, Math.pow(1.0015, wheel.angleDelta.y));
                    }
                    wheel.accepted = true;
                }
            }
        }
    }
    Label {
        anchors.left: parent.left
        anchors.margins: 16
        anchors.top: parent.top
        color: "#93a6bc"
        text: "二维画布"
    }
    Button {
        anchors.margins: 10
        anchors.right: parent.right
        anchors.top: parent.top
        text: "适配视图"

        onClicked: canvas.fitView()
    }
    Label {
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.margins: 16
        color: "#93a6bc"
        font.pixelSize: 12
        text: canvas.editor.courseWeek >= 4 ? "滚轮缩放 · 中键平移 · 左键拖动\n" + canvas.zoom.toFixed(2) + " px / mm" : "X → · Y ↑ · " + canvas.zoom.toFixed(2) + " px / mm"
    }
    Connections {
        function onDocumentReplaced() {
            Qt.callLater(canvas.fitView);
        }

        target: canvas.editor
    }
}
