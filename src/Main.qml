import QtCore
import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

ApplicationWindow {
    id: win
    width: 440
    height: 660
    minimumWidth: 360
    minimumHeight: 300
    visible: true
    title: "scicalc"
    color: "#0e0e10"

    Material.theme: Material.Dark
    Material.accent: theme.accent
    Material.roundedScale: theme.radius

    readonly property string mono: "monospace"
    readonly property color dim: "#8a8a93"
    readonly property color faint: "#2a2a30"
    readonly property color errorColor: "#ff6b6b"
    readonly property int smallRadius: Math.min(4, theme.radius)

    property bool helpVisible: false
    // Up/Down walk the history like a shell; -1 means editing a fresh line.
    property int recallIndex: -1
    property string draft: ""

    Settings {
        id: settings
        category: "window"
        property bool keypad: true
    }

    function insert(text) {
        if (input.selectedText.length > 0)
            input.remove(input.selectionStart, input.selectionEnd)
        input.insert(input.cursorPosition, text)
        backend.clearError()
        input.forceActiveFocus()
    }

    function backspace() {
        if (input.selectedText.length > 0)
            input.remove(input.selectionStart, input.selectionEnd)
        else if (input.cursorPosition > 0)
            input.remove(input.cursorPosition - 1, input.cursorPosition)
        backend.clearError()
    }

    function submit() {
        if (backend.evaluate(input.text)) {
            input.text = ""
            recallIndex = -1
            draft = ""
        } else if (backend.errorPosition >= 0) {
            input.cursorPosition = Math.min(backend.errorPosition, input.text.length)
        }
    }

    function recall(step) {
        const count = backend.history.length
        if (count === 0)
            return
        if (recallIndex === -1) {
            if (step > 0)
                return
            draft = input.text
            recallIndex = count - 1
        } else {
            recallIndex += step
        }
        if (recallIndex >= count) {
            recallIndex = -1
            input.text = draft
        } else {
            recallIndex = Math.max(0, recallIndex)
            input.text = backend.history[recallIndex].input
            history.positionViewAtIndex(recallIndex, ListView.Contain)
        }
        backend.clearError()
    }

    function copyLast() {
        if (backend.lastResult.length > 0)
            backend.copy(backend.lastResult)
    }

    Item {
        anchors.fill: parent
        LayoutMirroring.enabled: Qt.application.layoutDirection === Qt.RightToLeft
        LayoutMirroring.childrenInherit: true

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 12
            spacing: 8

            // Mode and variables.
            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                Rectangle {
                    implicitWidth: modeLabel.implicitWidth + 16
                    implicitHeight: 24
                    radius: win.smallRadius
                    color: modeArea.containsMouse ? Qt.lighter(theme.accent, 1.15) : theme.accent

                    Text {
                        id: modeLabel
                        anchors.centerIn: parent
                        text: backend.degrees ? "DEG" : "RAD"
                        color: theme.accentForeground
                        font.family: win.mono
                        font.pixelSize: 12
                        font.bold: true
                    }
                    MouseArea {
                        id: modeArea
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: backend.degrees = !backend.degrees
                    }
                    ToolTip.visible: modeArea.containsMouse
                    ToolTip.delay: 600
                    ToolTip.text: qsTr("Angle mode (Ctrl+D)")
                }

                Flickable {
                    Layout.fillWidth: true
                    implicitHeight: 24
                    contentWidth: varsRow.implicitWidth
                    clip: true
                    boundsBehavior: Flickable.StopAtBounds

                    Row {
                        id: varsRow
                        height: parent.height
                        spacing: 10
                        Repeater {
                            model: backend.variables
                            Text {
                                anchors.verticalCenter: parent.verticalCenter
                                textFormat: Text.PlainText
                                text: modelData.name + "=" + backend.grouped(modelData.value)
                                color: varArea.containsMouse ? "white" : win.dim
                                font.family: win.mono
                                font.pixelSize: 12
                                MouseArea {
                                    id: varArea
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: win.insert(modelData.name)
                                }
                            }
                        }
                    }
                }

                Text {
                    text: qsTr("? help")
                    color: win.dim
                    font.family: win.mono
                    font.pixelSize: 12
                }
            }

            // The TI home screen: each entry on the left, its answer on the right.
            ListView {
                id: history
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                model: backend.history
                spacing: 2
                boundsBehavior: Flickable.StopAtBounds
                ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
                onCountChanged: Qt.callLater(positionViewAtEnd)
                Component.onCompleted: positionViewAtEnd()

                delegate: Rectangle {
                    id: entry
                    required property var modelData
                    required property int index
                    width: ListView.view.width
                    implicitHeight: entryColumn.implicitHeight + 10
                    radius: win.smallRadius
                    color: index === win.recallIndex ? win.faint
                         : entryHover.hovered ? "#18181c" : "transparent"

                    HoverHandler { id: entryHover }

                    Column {
                        id: entryColumn
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.leftMargin: 6
                        anchors.rightMargin: 10

                        Text {
                            width: parent.width
                            textFormat: Text.PlainText
                            text: entry.modelData.input
                            color: inputArea.containsMouse ? "white" : win.dim
                            font.family: win.mono
                            font.pixelSize: 15
                            wrapMode: Text.WrapAnywhere
                            MouseArea {
                                id: inputArea
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: win.insert(entry.modelData.input)
                            }
                        }

                        Row {
                            anchors.right: parent.right
                            spacing: 12
                            layoutDirection: Qt.LeftToRight

                            Text {
                                anchors.baseline: resultText.baseline
                                visible: entry.modelData.assigned.length > 0
                                textFormat: Text.PlainText
                                text: "→ " + entry.modelData.assigned
                                color: win.dim
                                font.family: win.mono
                                font.pixelSize: 13
                            }
                            Text {
                                anchors.baseline: resultText.baseline
                                visible: entry.modelData.fraction.length > 0
                                textFormat: Text.PlainText
                                text: entry.modelData.fraction
                                color: fractionArea.containsMouse ? "white" : win.dim
                                font.family: win.mono
                                font.pixelSize: 14
                                MouseArea {
                                    id: fractionArea
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: win.insert("(" + entry.modelData.fraction + ")")
                                }
                            }
                            Text {
                                id: resultText
                                textFormat: Text.PlainText
                                // Shown grouped, inserted plain, so it reads back.
                                text: backend.grouped(entry.modelData.result)
                                color: resultArea.containsMouse ? theme.accent : "white"
                                font.family: win.mono
                                font.pixelSize: 24
                                font.bold: true
                                MouseArea {
                                    id: resultArea
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: win.insert(entry.modelData.result)
                                }
                            }
                        }
                    }
                }

                Text {
                    anchors.centerIn: parent
                    width: parent.width - 40
                    visible: history.count === 0
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WordWrap
                    text: qsTr("Type an expression and press Enter.\nStart with an operator to continue from the last answer.")
                    color: win.dim
                    font.pixelSize: 13
                    lineHeight: 1.3
                }
            }

            // The entry line.
            Rectangle {
                Layout.fillWidth: true
                implicitHeight: 48
                radius: win.smallRadius
                color: "#16161a"
                border.width: backend.error.length > 0 ? 2 : 1
                border.color: backend.error.length > 0 ? win.errorColor
                            : input.activeFocus ? theme.accent : win.faint

                TextInput {
                    id: input
                    anchors.fill: parent
                    anchors.leftMargin: 12
                    anchors.rightMargin: 12
                    verticalAlignment: TextInput.AlignVCenter
                    focus: true
                    clip: true
                    color: "white"
                    selectionColor: theme.accent
                    selectedTextColor: theme.accentForeground
                    font.family: win.mono
                    font.pixelSize: 20
                    selectByMouse: true
                    onTextEdited: { backend.clearError(); win.recallIndex = -1 }

                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        visible: input.text.length === 0
                        text: backend.history.length > 0 ? qsTr("ans = %1").arg(backend.grouped(backend.lastResult)) : "2π·sin(30)"
                        color: "#55555c"
                        font: input.font
                        textFormat: Text.PlainText
                    }

                    // Marks the character the error points at.
                    Rectangle {
                        visible: backend.error.length > 0 && backend.errorPosition >= 0
                        readonly property rect at: input.positionToRectangle(Math.min(backend.errorPosition, input.text.length))
                        x: at.x
                        y: at.y + at.height + 1
                        width: Math.max(8, input.positionToRectangle(Math.min(backend.errorPosition + 1, input.text.length)).x - at.x)
                        height: 2
                        color: win.errorColor
                    }

                    Keys.onPressed: (event) => {
                        const ctrl = event.modifiers & Qt.ControlModifier
                        const shift = event.modifiers & Qt.ShiftModifier
                        if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
                            win.submit()
                        } else if (event.key === Qt.Key_Escape) {
                            if (win.helpVisible) win.helpVisible = false
                            else if (input.text.length > 0 || backend.error.length > 0) {
                                input.text = ""; backend.clearError(); win.recallIndex = -1
                            } else Qt.quit()
                        } else if (event.key === Qt.Key_Up) {
                            win.recall(-1)
                        } else if (event.key === Qt.Key_Down) {
                            win.recall(1)
                        } else if (event.text === "?" || event.key === Qt.Key_F1) {
                            win.helpVisible = !win.helpVisible
                        } else if (ctrl && event.key === Qt.Key_D) {
                            backend.degrees = !backend.degrees
                        } else if (ctrl && event.key === Qt.Key_K) {
                            settings.keypad = !settings.keypad
                        } else if (ctrl && event.key === Qt.Key_L) {
                            backend.clearHistory(); win.recallIndex = -1
                        } else if (ctrl && event.key === Qt.Key_Q) {
                            Qt.quit()
                        } else if (ctrl && event.key === Qt.Key_C && (shift || input.selectedText.length === 0)) {
                            win.copyLast()
                        } else {
                            return
                        }
                        event.accepted = true
                    }
                }
            }

            Text {
                Layout.fillWidth: true
                Layout.preferredHeight: 16
                textFormat: Text.PlainText
                text: backend.error
                color: win.errorColor
                font.pixelSize: 13
                elide: Text.ElideRight
            }

            // The keypad, for when the mouse is already in hand.
            GridLayout {
                id: keypad
                visible: settings.keypad
                Layout.fillWidth: true
                columns: 7
                rowSpacing: 5
                columnSpacing: 5
                // Number pad always reads left to right, even when mirrored.
                LayoutMirroring.enabled: false

                Repeater {
                    model: [
                        { label: "sin", text: "sin(", kind: "fn" },
                        { label: "cos", text: "cos(", kind: "fn" },
                        { label: "tan", text: "tan(", kind: "fn" },
                        { label: "(", text: "(", kind: "op" },
                        { label: ")", text: ")", kind: "op" },
                        { label: "⌫", action: "back", kind: "act" },
                        { label: "AC", action: "clear", kind: "act" },

                        { label: "sin⁻¹", text: "asin(", kind: "fn" },
                        { label: "cos⁻¹", text: "acos(", kind: "fn" },
                        { label: "tan⁻¹", text: "atan(", kind: "fn" },
                        { label: "7", text: "7", kind: "num" },
                        { label: "8", text: "8", kind: "num" },
                        { label: "9", text: "9", kind: "num" },
                        { label: "÷", text: "/", kind: "op" },

                        { label: "ln", text: "ln(", kind: "fn" },
                        { label: "log", text: "log(", kind: "fn" },
                        { label: "eˣ", text: "e^", kind: "fn" },
                        { label: "4", text: "4", kind: "num" },
                        { label: "5", text: "5", kind: "num" },
                        { label: "6", text: "6", kind: "num" },
                        { label: "×", text: "*", kind: "op" },

                        { label: "x²", text: "²", kind: "fn" },
                        { label: "xʸ", text: "^", kind: "fn" },
                        { label: "√", text: "√(", kind: "fn" },
                        { label: "1", text: "1", kind: "num" },
                        { label: "2", text: "2", kind: "num" },
                        { label: "3", text: "3", kind: "num" },
                        { label: "−", text: "-", kind: "op" },

                        { label: "π", text: "π", kind: "fn" },
                        { label: "e", text: "e", kind: "fn" },
                        { label: "n!", text: "!", kind: "fn" },
                        { label: "0", text: "0", kind: "num" },
                        { label: ".", text: ".", kind: "num" },
                        { label: "ans", text: "ans", kind: "num" },
                        { label: "+", text: "+", kind: "op" },

                        { label: "nCr", text: "nCr(", kind: "fn" },
                        { label: "→", text: "→", kind: "fn" },
                        { label: ",", text: ",", kind: "fn" },
                        { label: "EE", text: "e", kind: "fn" },
                        { label: "(−)", text: "-", kind: "num" },
                        { label: "1/x", text: "^-1", kind: "num" },
                        { label: "=", action: "eval", kind: "eval" }
                    ]

                    delegate: Rectangle {
                        id: key
                        required property var modelData
                        Layout.fillWidth: true
                        Layout.preferredWidth: 1
                        Layout.preferredHeight: 40
                        radius: win.smallRadius
                        readonly property color base: modelData.kind === "eval" ? theme.accent
                                                    : modelData.kind === "num" ? "#2a2a31"
                                                    : modelData.kind === "act" ? "#3a2a2e"
                                                    : modelData.kind === "op" ? "#24242b" : "#1a1a1f"
                        color: keyArea.pressed ? Qt.lighter(base, 1.4) : keyArea.containsMouse ? Qt.lighter(base, 1.2) : base

                        Text {
                            anchors.centerIn: parent
                            text: key.modelData.label
                            color: key.modelData.kind === "eval" ? theme.accentForeground
                                 : key.modelData.kind === "fn" ? "#c9c9d1" : "white"
                            font.family: win.mono
                            font.pixelSize: key.modelData.kind === "num" || key.modelData.kind === "op" ? 18 : 14
                            font.bold: key.modelData.kind === "eval"
                        }
                        MouseArea {
                            id: keyArea
                            anchors.fill: parent
                            hoverEnabled: true
                            onClicked: {
                                const a = key.modelData.action
                                if (a === "back") win.backspace()
                                else if (a === "clear") { input.text = ""; backend.clearError(); win.recallIndex = -1 }
                                else if (a === "eval") win.submit()
                                else win.insert(key.modelData.text)
                                input.forceActiveFocus()
                            }
                        }
                    }
                }
            }
        }

        // Every shortcut and function, on '?'.
        Rectangle {
            anchors.fill: parent
            visible: win.helpVisible
            color: win.color

            MouseArea { anchors.fill: parent; onClicked: win.helpVisible = false }

            Flickable {
                anchors.fill: parent
                anchors.margins: 20
                contentHeight: helpColumn.implicitHeight
                clip: true

                Column {
                    id: helpColumn
                    width: parent.width
                    spacing: 14

                    Text {
                        text: qsTr("Keyboard")
                        color: theme.accent
                        font.pixelSize: 16
                        font.bold: true
                    }

                    Column {
                        width: parent.width
                        spacing: 6
                        Repeater {
                            model: [
                                ["Enter", qsTr("Calculate (repeats the last entry when empty)")],
                                ["↑ / ↓", qsTr("Recall earlier entries")],
                                ["Esc", qsTr("Clear the line, or close when empty")],
                                ["Ctrl+D", qsTr("Switch degrees / radians")],
                                ["Ctrl+C", qsTr("Copy the last answer (with nothing selected)")],
                                ["Ctrl+K", qsTr("Show or hide the keypad")],
                                ["Ctrl+L", qsTr("Clear the history")],
                                ["Ctrl+Q", qsTr("Quit")],
                                ["?", qsTr("Show or hide this help")]
                            ]
                            Row {
                                required property var modelData
                                spacing: 12
                                Text {
                                    width: 70
                                    text: modelData[0]
                                    color: "white"
                                    font.family: win.mono
                                    font.pixelSize: 13
                                }
                                Text {
                                    width: helpColumn.width - 82
                                    wrapMode: Text.WordWrap
                                    text: modelData[1]
                                    color: win.dim
                                    font.pixelSize: 13
                                }
                            }
                        }
                    }

                    Text {
                        text: qsTr("Writing expressions")
                        color: theme.accent
                        font.pixelSize: 16
                        font.bold: true
                    }

                    Column {
                        width: parent.width
                        spacing: 6
                        Repeater {
                            model: [
                                ["2pi  3(4+1)", qsTr("Multiplication can be implied")],
                                ["*2  ^2  !", qsTr("A leading operator continues from ans")],
                                ["x = 5   5→x", qsTr("Store a variable (-> works too)")],
                                ["2^10  2**10", qsTr("Powers; -2^2 is -4")],
                                ["7 % 3", qsTr("Remainder (also mod(7,3))")],
                                ["1.5e3", qsTr("Scientific notation")],
                                ["0xff 0b101", qsTr("Hex, binary and octal (0o) numbers")],
                                ["sin(30", qsTr("Closing parentheses are optional")]
                            ]
                            Row {
                                required property var modelData
                                spacing: 12
                                Text {
                                    width: 110
                                    text: modelData[0]
                                    color: "white"
                                    font.family: win.mono
                                    font.pixelSize: 13
                                }
                                Text {
                                    width: helpColumn.width - 122
                                    wrapMode: Text.WordWrap
                                    text: modelData[1]
                                    color: win.dim
                                    font.pixelSize: 13
                                }
                            }
                        }
                    }

                    Text {
                        text: qsTr("Functions and constants")
                        color: theme.accent
                        font.pixelSize: 16
                        font.bold: true
                    }

                    Text {
                        width: parent.width
                        wrapMode: Text.WordWrap
                        textFormat: Text.PlainText
                        text: backend.functionNames().join("  ") + "\n\npi  π  tau  e  phi  ans"
                        color: win.dim
                        font.family: win.mono
                        font.pixelSize: 12
                        lineHeight: 1.4
                    }
                }
            }
        }
    }
}
