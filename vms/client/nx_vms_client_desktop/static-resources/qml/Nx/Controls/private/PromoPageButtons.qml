import QtQuick 2.11
import QtQuick.Controls 2.4
import QtQuick.Layouts 1.11

import Nx 1.0
import Nx.Controls 1.0

Control
{
    id: buttons

    padding: 16

    property SwipeView pages: null

    signal closeRequested()

    contentItem: Item
    {
        id: bar

        implicitWidth: prevButton.width + nextButton.width + closeButton.width + 16
        implicitHeight: 20

        readonly property bool isFirstPage: pages && (pages.currentIndex === 0)
        readonly property bool isLastPage: pages && (pages.currentIndex === pages.count - 1)

        TextButton
        {
            id: prevButton

            icon.source: "qrc:///skin/text_buttons/arrow_left.png"
            color: pressed || hovered ? ColorTheme.colors.light4 : ColorTheme.colors.light9
            visible: !bar.isFirstPage

            anchors.verticalCenter: bar.verticalCenter

            onClicked:
                pages.decrementCurrentIndex()
        }

        Button
        {
            id: nextButton

            text: bar.isLastPage ? qsTr("OK") : qsTr("Next")
            textColor: pressed || hovered ? ColorTheme.colors.light4 : ColorTheme.colors.light9

            leftPadding: 12
            rightPadding: 12
            height: 24
            width: Math.max(implicitWidth, 56)
            anchors.centerIn: bar

            background: Rectangle
            {
                radius: 2

                border.color: nextButton.textColor

                color:
                {
                    if (nextButton.pressed)
                        return ColorTheme.transparent(ColorTheme.colors.dark1, 0.1)

                    return nextButton.hovered
                        ? ColorTheme.transparent(ColorTheme.colors.light4, 0.1)
                        : "transparent"
                }
            }

            onClicked:
            {
                if (bar.isLastPage)
                    buttons.closeRequested()
                else
                    pages.incrementCurrentIndex()
            }
        }

        TextButton
        {
            id: closeButton

            icon.source: "qrc:///skin/text_buttons/clear.png"
            color: pressed || hovered ? ColorTheme.colors.light4 : ColorTheme.colors.light9

            anchors.verticalCenter: bar.verticalCenter
            anchors.right: bar.right

            onClicked:
                buttons.closeRequested()
        }
    }
}
