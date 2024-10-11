// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "text_utils.h"

#include <QtGui/QTextDocument>
#include <QtGui/QTextBlock>
#include <QtGui/QTextCursor>

namespace nx::vms::client::core {
namespace text_utils {

void elideDocumentHeight(QTextDocument* document, int maxHeight, const QString& tail)
{
    // This line is required to force the document to create the layout, which will then be used
    // to count the lines.
    document->documentLayout();

    std::optional<int> lastFullyVisiblePosition;

    // Iterate over document blocks and their lines to find first partially visible line.
    for (QTextBlock block = document->firstBlock(); block.isValid(); block = block.next())
    {
        const QTextLayout* layout = block.layout();
        for (int lineIndex = 0; lineIndex < layout->lineCount(); ++lineIndex)
        {
            const QTextLine line = layout->lineAt(lineIndex);

            const int documentPosition = block.position() + line.textStart();

            // Is line fully visible?
            if (layout->position().y() + line.naturalTextRect().bottom() <= maxHeight)
            {
                // Remember it and go to the next line.
                lastFullyVisiblePosition = documentPosition;
                continue;
            }

            // Found first partially visible line.

            if (!lastFullyVisiblePosition) //< First line is already partially visible.
                lastFullyVisiblePosition = documentPosition;

            // Replace last fully visible line with tail.
            QTextCursor cursor(document);
            cursor.setPosition(*lastFullyVisiblePosition);
            cursor.movePosition(QTextCursor::End, QTextCursor::KeepAnchor);
            cursor.insertText(tail);
            return;
        }
    }
}

void elideDocumentLines(
    QTextDocument* document, int maxLines, bool trimLastLine, const QString& tail)
{
    // This line is required to force the document to create the layout, which will then be used
    // to count the lines.
    document->documentLayout();

    std::optional<int> lastFullyVisiblePosition;
    qreal offset = 0;

    // Iterate over document blocks and their lines to find first partially visible line.
    for (QTextBlock block = document->firstBlock(); block.isValid(); block = block.next())
    {
        const QTextLayout* layout = block.layout();

        for (int lineIndex = 0; lineIndex < layout->lineCount(); ++lineIndex)
        {
            const QTextLine line = layout->lineAt(lineIndex);

            const int documentPosition = block.position() + line.textStart();

            if (lineIndex < maxLines)
            {
                offset = document->textWidth() - line.naturalTextWidth();
                // Remember it and go to the next line.
                lastFullyVisiblePosition = documentPosition;
                continue;
            }

            // Found extra lines.

            if (!lastFullyVisiblePosition) //< First line is already partially visible.
                lastFullyVisiblePosition = documentPosition;

            // Replace last fully visible line with tail.
            QTextCursor cursor(document);
            if (trimLastLine)
            {
                cursor.setPosition(line.xToCursor(offset));
                cursor.movePosition(QTextCursor::PreviousCharacter, QTextCursor::MoveAnchor, tail.size());
            }
            else
            {
                cursor.setPosition(*lastFullyVisiblePosition);
            }
            cursor.movePosition(QTextCursor::End, QTextCursor::KeepAnchor);
            cursor.insertText(tail);
            return;
        }
    }
}

void elideTextRight(QTextDocument* document, int width, const QString& tail)
{
    // This line is required to force the document to create the layout, which will then be used
    // to count the lines.
    document->documentLayout();

    QTextBlock block = document->firstBlock();
    if (!block.isValid())
        return;

    const QTextLayout* layout = block.layout();

    if (layout->lineCount() <= 0)
        return;

    const QTextLine line = layout->lineAt(0);

    if (document->idealWidth() <= width)
        return;

    // Cut the line at required width.
    auto x = line.xToCursor(width, QTextLine::CursorOnCharacter);

    QTextCursor cursor(document);

    cursor.setPosition(x);
    cursor.movePosition(QTextCursor::End, QTextCursor::KeepAnchor);
    cursor.removeSelectedText();

    // Add the tail.
    cursor.insertText(tail);

    // Remove characters before the tail to fit the line into required width.
    cursor.setPosition(x);
    while (document->idealWidth() > width && cursor.position() > 0)
        cursor.deletePreviousChar();
}

} // namespace text_utils
} // namespace nx::vms::client::core
