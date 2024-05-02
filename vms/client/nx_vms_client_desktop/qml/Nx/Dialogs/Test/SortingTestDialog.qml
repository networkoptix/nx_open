import QtQuick

import Nx.Controls
import Nx.Core
import Nx.Dialogs

Dialog
{
    id: dialog

    fixedWidth: implicitWidth
    fixedHeight: implicitHeight

    Collator
    {
        id: collator

        caseSensitivity: caseButton.checked ? Qt.CaseInsensitive : Qt.CaseSensitive
        numericMode: numericModeButton.checked
        ignorePunctuation: ignorePunctuationButton.checked

        onChanged:
            dialog.sort()
    }

    function sort()
    {
        let lines = sourceLines.text.split("\n")
        lines.sort(collator.compare)
        sortedLines.text = lines.join("\n")
    }

    contentItem: Column
    {
        spacing: 32

        Grid
        {
            columns: 2
            rows: 2

            rowSpacing: 8
            columnSpacing: 16

            flow: Grid.TopToBottom

            Label
            {
                text: "Source lines:"
            }

            Scrollable
            {
                width: 200
                height: 300

                contentItem: TextArea
                {
                    id: sourceLines

                    onTextChanged:
                        dialog.sort()
                }
            }

            Label
            {
                text: "Sorted lines:"
            }

            Scrollable
            {
                width: 200
                height: 300

                contentItem: TextArea
                {
                    id: sortedLines

                    readOnly: true
                }
            }
        }

        Row
        {
            spacing: 16

            Column
            {
                width: sourceLines.width

    	        RadioButton
                {
                    id: caseButton

                    text: "Case Insensitive"
                    checked: true
                    topPadding: 3
                    bottomPadding: 2
                }

    	        RadioButton
                {
                    text: "Case Sensitive"
                    topPadding: 3
                    bottomPadding: 2
                }
            }

            Column
            {
    	        CheckBox
                {
                    id: numericModeButton

                    text: "Numeric Mode"
                    checked: true
                }

    	        CheckBox
                {
                    id: ignorePunctuationButton

                    text: "Ignore Punctuation"
                }
            }
        }
    }

    buttonBox: DialogButtonBox
    {
        standardButtons: DialogButtonBox.Close
    }
}
