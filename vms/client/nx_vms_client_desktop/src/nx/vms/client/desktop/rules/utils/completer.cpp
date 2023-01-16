// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "completer.h"

#include <QtCore/QScopedValueRollback>
#include <QtGui/QKeyEvent>
#include <QtWidgets/QAbstractItemView>

namespace nx::vms::client::desktop::rules {

Completer::Completer(const QStringList& words, QLineEdit* lineEdit, QObject* parent):
    QObject(parent), m_completer{words}
{
    m_completer.setCaseSensitivity(Qt::CaseInsensitive);
    m_completer.setWidget(lineEdit);

    connect(&m_completer,
        QOverload<const QString&>::of(&QCompleter::activated),
        this,
        [this, lineEdit](const QString& completedText)
        {
            auto currentText = lineEdit->text();
            replaceCurrentWordWithCompleted(currentText, completedText);

            QScopedValueRollback rollback{m_isCompletingInProgress, true};
            lineEdit->setText(currentText);
            lineEdit->setCursorPosition(m_currentWord.begin + completedText.size());
        });

    const auto triggerCompletion =
        [this, lineEdit]
        {
            if (m_isCompletingInProgress)
                return;

            updateCompleter(lineEdit->cursorPosition(), lineEdit->text());
            m_completer.complete(lineEdit->rect());
        };

    connect(lineEdit, &QLineEdit::textChanged, this, triggerCompletion);
    connect(lineEdit, &QLineEdit::cursorPositionChanged, this, triggerCompletion);
}

Completer::Completer(const QStringList& words, QTextEdit* textEdit, QObject* parent):
    QObject(parent), m_completer{words}
{
    m_completer.setCaseSensitivity(Qt::CaseInsensitive);
    m_completer.setWidget(textEdit);
    m_completer.popup()->installEventFilter(this);

    connect(&m_completer,
        QOverload<const QString&>::of(&QCompleter::activated),
        this,
        [this, textEdit](const QString& completedText)
        {
            auto currentText = textEdit->toPlainText();
            replaceCurrentWordWithCompleted(currentText, completedText);

            QScopedValueRollback rollback{m_isCompletingInProgress, true};
            textEdit->setText(currentText);

            auto cursor = textEdit->textCursor();
            cursor.setPosition(m_currentWord.begin + completedText.size());
            textEdit->setTextCursor(cursor);
        });

    const auto triggerCompleter =
        [this, textEdit]
        {
            if (m_isCompletingInProgress)
                return;

            updateCompleter(textEdit->textCursor().position(), textEdit->toPlainText());

            const auto cursorRect =
                textEdit->cursorRect().adjusted(0, 0, 0, textEdit->fontMetrics().descent());
            const auto textEditrect = textEdit->rect();
            m_completer.complete(QRect{
                QPoint{textEditrect.left(), cursorRect.top()},
                QPoint{textEditrect.right(), cursorRect.bottom()}});
        };

    connect(textEdit, &QTextEdit::textChanged, this, triggerCompleter);
    connect(textEdit, &QTextEdit::cursorPositionChanged, this, triggerCompleter);
}

bool Completer::eventFilter(QObject* obj, QEvent* event)
{
    if (event->type() == QEvent::KeyPress)
    {
        // It is required to handle such event here, otherwise new line is added in the text
        // editor instead of applying choosen completion string.
        auto keyEvent = static_cast<QKeyEvent*>(event);
        switch (keyEvent->key())
        {
            case Qt::Key_Enter:
            case Qt::Key_Return:
                m_completer.popup()->hide();
                emit m_completer.activated(m_completer.popup()->currentIndex().data().toString());

                return true;
            default:
                break;
        }
    }

    return QObject::eventFilter(obj, event);
}

void Completer::updateCompleter(int cursorPosition, const QString& text)
{
    static const QRegularExpression spacePattern{"\\s", QRegularExpression::CaseInsensitiveOption};

    QStringView textBeforeCursor{text.data(), cursorPosition};
    auto spaceBeforeWord = textBeforeCursor.lastIndexOf(spacePattern);
    m_currentWord.begin = spaceBeforeWord == -1 ? 0 : (spaceBeforeWord + 1);

    auto spaceAfterWord = text.indexOf(spacePattern, cursorPosition);
    auto wordEnd = spaceAfterWord == -1 ? text.size() : spaceAfterWord;
    m_currentWord.length = wordEnd - m_currentWord.begin;

    auto currentWordSlice =
        textBeforeCursor.sliced(m_currentWord.begin, cursorPosition - m_currentWord.begin);

    m_completer.setCompletionPrefix(currentWordSlice.toString());
}

void Completer::replaceCurrentWordWithCompleted(QString& text, const QString& completedText) const
{
    if (m_currentWord.length == 0)
    {
        text.insert(m_currentWord.begin, completedText);
    }
    else
    {
        text.remove(m_currentWord.begin, m_currentWord.length);
        text.insert(m_currentWord.begin, completedText);
    }
}

} // namespace nx::vms::client::desktop::rules
