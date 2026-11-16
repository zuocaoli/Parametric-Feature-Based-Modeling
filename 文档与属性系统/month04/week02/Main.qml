pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs

ApplicationWindow {
    id: window

    property bool allowClose: false
    required property var editor

    color: "#101720"
    font.family: "Noto Sans"
    font.pixelSize: 14
    height: 800
    palette.base: "#17222f"
    palette.button: "#243244"
    palette.buttonText: "#e0e9f2"
    palette.highlight: "#55b5d9"
    palette.text: "#e0e9f2"
    palette.window: "#101720"
    palette.windowText: "#e0e9f2"
    title: (editor.modified ? "* " : "") + "Learn CAD · 第六个月 / 第 " + editor.courseWeek + " 周"
    visible: true
    width: 1220

    footer: Rectangle {
        color: "#17222f"
        height: statusText.implicitHeight + 24

        Label {
            id: statusText

            anchors.fill: parent
            anchors.margins: 12
            color: window.editor.errorMessage ? "#ffc078" : "#93a6bc"
            text: window.editor.errorMessage || (window.editor.previewId ? "临时位置预览 · 松开提交 / Escape 取消" : "就绪 · 单位 mm · 历史位置 " + window.editor.historyPosition)
            wrapMode: Text.WordWrap
        }
    }
    header: ToolBar {
        height: 74

        background: Rectangle {
            color: "#17222f"
        }

        RowLayout {
            anchors.fill: parent
            anchors.margins: 16

            Label {
                color: "#79d2ed"
                font.bold: true
                font.pixelSize: 20
                text: "▣  LEARN CAD"
            }
            Label {
                Layout.leftMargin: 12
                color: "#a1b1c5"
                text: window.editor.modified ? "文档未保存" : (window.editor.hasDrafts ? "有未提交的公式草稿" : "参数化二维编辑器")
            }
            Item {
                Layout.fillWidth: true
            }
            Button {
                text: "新建矩形"
                visible: true

                onClicked: window.editor.createRectangle()
            }
            Button {
                enabled: window.editor.canUndo
                text: "撤销"
                visible: true

                onClicked: window.editor.undo()
            }
            Button {
                enabled: window.editor.canRedo
                text: "重做"
                visible: true

                onClicked: window.editor.redo()
            }
            Button {
                text: "打开"
                visible: true

                onClicked: {
                    if (window.editor.modified || window.editor.hasDrafts) {
                        discardDialog.closingWindow = false;
                        discardDialog.open();
                    } else
                        openDialog.open();
                }
            }
            Button {
                text: "保存"
                visible: true

                onClicked: saveDialog.open()
            }
        }
    }

    onClosing: close => {
        if (!allowClose && (editor.modified || editor.hasDrafts)) {
            close.accepted = false;
            discardDialog.closingWindow = true;
            discardDialog.open();
        }
    }

    RowLayout {
        anchors.fill: parent
        anchors.margins: 14
        spacing: 14

        Rectangle {
            Layout.fillHeight: true
            Layout.preferredWidth: 206
            color: "#17222f"
            radius: 8

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 14

                Label {
                    font.bold: true
                    font.pixelSize: 17
                    text: "文档对象"
                }
                Label {
                    color: "#93a6bc"
                    font.pixelSize: 12
                    text: "稳定 ID · 共享选择"
                }
                ListView {
                    id: objectList

                    Layout.fillHeight: true
                    Layout.fillWidth: true
                    clip: true
                    model: window.editor
                    objectName: "objectList"
                    spacing: 8

                    delegate: ItemDelegate {
                        required property string objectId
                        required property string objectNameText
                        required property bool stale

                        enabled: true
                        highlighted: window.editor.selectedId === objectId
                        text: objectNameText + "  #" + objectId + (stale ? " ⚠" : "")
                        width: ListView.view.width

                        onClicked: window.editor.select(objectId)
                    }
                }
                Button {
                    Layout.fillWidth: true
                    enabled: window.editor.selectedId !== ""
                    text: "删除选中对象"
                    visible: true

                    onClicked: window.editor.deleteSelected()
                }
            }
        }
        CanvasView {
            id: canvas

            Layout.fillHeight: true
            Layout.fillWidth: true
            Layout.minimumWidth: 320
            editor: window.editor
            objectName: "canvas"
        }
        Rectangle {
            Layout.fillHeight: true
            Layout.preferredWidth: 338
            color: "#17222f"
            radius: 8

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 14

                Label {
                    font.bold: true
                    font.pixelSize: 17
                    text: "属性 / 公式"
                }
                Label {
                    color: "#93a6bc"
                    text: window.editor.selectedId ? "对象 #" + window.editor.selectedId + " · mm" : "点击画布或对象列表"
                }
                Inspector {
                    Layout.fillHeight: true
                    Layout.fillWidth: true
                    editor: window.editor
                    visible: true
                }
                ColumnLayout {
                    Layout.fillWidth: true
                    visible: false

                    Label {
                        Layout.fillWidth: true
                        text: "修改核心 → 重算 → 显示更新"
                        wrapMode: Text.WordWrap
                    }
                    Button {
                        text: "Length 设为 140 mm"

                        onClicked: {
                            window.editor.select("1");
                            window.editor.setValue("length", "140");
                        }
                    }
                    Button {
                        text: "Length 恢复 100 mm"

                        onClicked: {
                            window.editor.select("1");
                            window.editor.setValue("length", "100");
                        }
                    }
                    Item {
                        Layout.fillHeight: true
                    }
                }
            }
        }
    }
    Dialog {
        id: discardDialog

        property bool closingWindow: false

        anchors.centerIn: parent
        modal: true
        standardButtons: Dialog.Discard | Dialog.Cancel
        title: "文档或草稿尚未保存"

        onDiscarded: {
            if (closingWindow) {
                window.allowClose = true;
                window.close();
            } else
                openDialog.open();
        }

        Label {
            text: "放弃未保存内容并继续？返回后可先保存文档、提交或放弃草稿。"
        }
    }
    FileDialog {
        id: openDialog

        nameFilters: ["CAD 文档 (*.cad)", "所有文件 (*)"]
        title: "打开 CAD_DOCUMENT 2"

        onAccepted: window.editor.openFile(selectedFile)
    }
    FileDialog {
        id: saveDialog

        defaultSuffix: "cad"
        fileMode: FileDialog.SaveFile
        nameFilters: ["CAD 文档 (*.cad)"]
        title: "保存 CAD_DOCUMENT 2"

        onAccepted: window.editor.saveFile(selectedFile)
    }
    Shortcut {
        enabled: true
        sequence: "Ctrl+Z"

        onActivated: window.editor.undo()
    }
    Shortcut {
        enabled: true
        sequence: "Ctrl+Shift+Z"

        onActivated: window.editor.redo()
    }
    Shortcut {
        sequence: "Escape"

        onActivated: window.editor.cancelDrag()
    }
    Connections {
        function onActiveChanged() {
            if (!window.active)
                window.editor.cancelDrag();
        }

        target: window
    }
}
