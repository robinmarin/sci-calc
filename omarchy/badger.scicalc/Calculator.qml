import QtQuick
import qs.Commons
import qs.Ui

// A calculator glyph that toggles scicalc: the first click opens it, the next
// closes it. scicalc itself keeps one window and remembers its history.
BarWidget {
  id: root
  moduleName: "badger.scicalc"

  implicitWidth: button.implicitWidth
  implicitHeight: button.implicitHeight

  BarIconButton {
    id: button
    anchors.fill: parent
    bar: root.bar
    text: ""
    slotSize: Style.bar.statusSlot
    tooltipText: "Calculator"
    onPressed: if (root.bar) root.bar.run("scicalc toggle")
  }
}
