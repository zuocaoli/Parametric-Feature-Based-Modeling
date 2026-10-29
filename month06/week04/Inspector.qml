pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ScrollView {
    id: panel

    required property var editor

    clip: true
    contentWidth: availableWidth

    ColumnLayout {
        spacing: 12
        width: panel.availableWidth

        Repeater {
            model: panel.editor.propertyRows

            delegate: ColumnLayout {
                id: row

                required property var modelData

                Layout.fillWidth: true
                spacing: 5

                Label {
                    font.bold: true
                    text: row.modelData.label + (row.modelData.unit ? "  / " + row.modelData.unit : "")
                }
                RowLayout {
                    Layout.fillWidth: true

                    TextField {
                        id: valueField

                        function submit() {
                            if (!readOnly && !panel.editor.setValue(row.modelData.key, text))
                                text = row.modelData.valueText;
                        }

                        Layout.fillWidth: true
                        color: readOnly ? "#93a6bc" : "#e0e9f2"
                        objectName: "value-" + row.modelData.key
                        readOnly: row.modelData.readOnly
                        selectByMouse: true
                        text: row.modelData.valueText
                        visible: row.modelData.type !== "bool"

                        onAccepted: submit()
                    }
                    CheckBox {
                        Layout.fillWidth: true
                        checked: row.modelData.valueText === "true"
                        enabled: !row.modelData.readOnly
                        text: "显示图形"
                        visible: row.modelData.type === "bool"

                        onClicked: panel.editor.setValue(row.modelData.key, checked ? "true" : "false")
                    }
                    Button {
                        text: "应用"
                        visible: row.modelData.type !== "bool" && !row.modelData.readOnly

                        onClicked: valueField.submit()
                    }
                }
                RowLayout {
                    Layout.fillWidth: true
                    visible: true && row.modelData.type === "double"

                    TextField {
                        id: formulaField

                        function submit() {
                            if (!panel.editor.setFormula(row.modelData.key, text) && panel.editor.errorStart >= 0) {
                                forceActiveFocus();
                                cursorPosition = panel.editor.errorStart;
                                select(panel.editor.errorStart, panel.editor.errorStart + panel.editor.errorLength);
                            }
                        }

                        Layout.fillWidth: true
                        objectName: "formula-" + row.modelData.key
                        placeholderText: "输入公式，例如 length / 2"
                        placeholderTextColor: "#93a6bc"
                        selectByMouse: true
                        text: row.modelData.editFormula

                        onAccepted: submit()
                        onTextEdited: panel.editor.editDraft(row.modelData.key, text)
                    }
                    Button {
                        text: "绑定"

                        onClicked: formulaField.submit()
                    }
                }
                Label {
                    Layout.fillWidth: true
                    color: "#ffc078"
                    text: (row.modelData.draftConflict ? "正式公式已变化；提交将覆盖它" : "公式草稿尚未提交") + "\n正式公式：" + (row.modelData.formula || "无")
                    visible: formulaField.text !== row.modelData.formula
                    wrapMode: Text.Wrap
                }
                Button {
                    text: "放弃公式草稿"
                    visible: formulaField.text !== row.modelData.formula

                    onClicked: panel.editor.discardDraft(row.modelData.key)
                }
                Button {
                    text: "解除公式，保留数值"
                    visible: true && row.modelData.bound

                    onClicked: panel.editor.clearFormula(row.modelData.key)
                }
                Label {
                    Layout.fillWidth: true
                    color: row.modelData.status === "Valid" ? "#93a6bc" : "#ffc078"
                    text: row.modelData.status + (row.modelData.reason ? " · " + row.modelData.reason : "")
                    visible: row.modelData.bound || row.modelData.status !== "Valid"
                    wrapMode: Text.WordWrap
                }
                Label {
                    Layout.fillWidth: true
                    color: "#ffc078"
                    text: panel.editor.errorMessage
                    visible: panel.editor.errorKey === row.modelData.key && panel.editor.errorMessage !== ""
                    wrapMode: Text.WordWrap
                }
                Rectangle {
                    Layout.fillWidth: true
                    color: "#2b3a4b"
                    implicitHeight: 1
                }
            }
        }
    }
}
