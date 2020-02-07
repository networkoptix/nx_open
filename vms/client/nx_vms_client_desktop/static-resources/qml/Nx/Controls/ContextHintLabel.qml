import QtQuick 2.6

Text
{
    id: contextHintLabel

    property int spacing: 4
    property alias contextHelpTopic: contextHintButton.helpTopic
    property alias contextHintText: contextHintButton.toolTipText

    property bool centerIfMultiline: true //< Center hint button vertically if label is multiline.

    readonly property bool hasContextHint: !!contextHintText

    rightPadding: hasContextHint ? contextHintButton.width + spacing : 0

    ContextHintButton
    {
        id: contextHintButton

        visible: hasContextHint
        x: parent.width - width

        y: centerIfMultiline && contextHintLabel.lineCount > 1
            ? (contextHintLabel.height - height) / 2
            : contextHintLabel.baselineOffset - baselineOffset
    }
}
