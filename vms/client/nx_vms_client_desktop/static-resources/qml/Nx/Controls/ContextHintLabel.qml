import QtQuick 2.6

Text
{
    id: contextHintLabel

    property int spacing: 4
    property alias contextHelpTopic: contextHintButton.helpTopic
    property alias contextHintText: contextHintButton.toolTipText

    rightPadding: contextHintButton.width + spacing

    ContextHintButton
    {
        id: contextHintButton
        x: parent.width - width
        y: parent.baselineOffset - baselineOffset
    }
}
