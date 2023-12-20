// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "completer.h"

#include <QtCore/QScopedValueRollback>
#include <QtGui/QKeyEvent>
#include <QtWidgets/QAbstractItemView>

#include <nx/vms/client/core/skin/color_theme.h>
#include <nx/vms/rules/action_builder_fields/text_with_fields.h>
#include <ui/common/palette.h>

#include "event_parameters_dropdown_delegate.h"

namespace nx::vms::client::desktop::rules {

struct Completer::Private: public QObject
{
    QCompleter* completer{nullptr};
    Completer* q;

    struct CurrentWord
    {
        int begin{0};
        int length{0};
    };

    CurrentWord currentWord;
    bool isCompletingInProgress{false};

    Private(Completer* parent): q(parent){};
    void updateCompleter(int cursorPosition, const QString& text);
    void replaceCurrentWordWithCompleted(QString& text, const QString& completedText) const;
    void initCompleterForTextEdit(QTextEdit* textEdit);
    void initPopup(QAbstractItemView* popup);
};

void Completer::Private::initPopup(QAbstractItemView* popup)
{
    popup->installEventFilter(q);
    popup->setItemDelegate(new EventParametersDropDownDelegate(q));
    setPaletteColor(popup, QPalette::Base, core::colorTheme()->color("dark13"));
    // Color of highlight and scroll.
    setPaletteColor(popup, QPalette::Midlight, core::colorTheme()->color("dark16"));
}

void Completer::Private::initCompleterForTextEdit(QTextEdit* textEdit)
{
    completer->setCaseSensitivity(Qt::CaseInsensitive);
    completer->setWidget(textEdit);
    initPopup(completer->popup());

    connect(completer,
        QOverload<const QString&>::of(&QCompleter::activated),
        q,
        [this, textEdit](const QString& completedText)
        {
            auto currentText = textEdit->toPlainText();
            replaceCurrentWordWithCompleted(currentText, completedText);

            QScopedValueRollback rollback{isCompletingInProgress, true};
            textEdit->setText(currentText);

            auto cursor = textEdit->textCursor();
            cursor.setPosition(currentWord.begin + completedText.size());
            textEdit->setTextCursor(cursor);
        });

    const auto triggerCompleter =
        [this, textEdit]
        {
            if (isCompletingInProgress)
                return;

            updateCompleter(textEdit->textCursor().position(), textEdit->toPlainText());

            const auto cursorRect =
                textEdit->cursorRect().adjusted(0, 0, 0, textEdit->fontMetrics().descent());
            const auto textEditrect = textEdit->rect();
            completer->complete(QRect{
                QPoint{textEditrect.left(), cursorRect.top()},
                QPoint{textEditrect.right(), cursorRect.bottom()}});
        };

    connect(textEdit, &QTextEdit::textChanged, q, triggerCompleter);
    connect(textEdit, &QTextEdit::cursorPositionChanged, q, triggerCompleter);
}

void Completer::Private::updateCompleter(int cursorPosition, const QString& text)
{
    static const QRegularExpression spacePattern{"\\s", QRegularExpression::CaseInsensitiveOption};

    const QStringView textBeforeCursor{text.data(), cursorPosition};
    const auto startOfSubstitution =
        textBeforeCursor.lastIndexOf(vms::rules::TextWithFields::kStartOfSubstitutionSymbol);
    currentWord.begin = startOfSubstitution == -1 ? 0 : startOfSubstitution;

    const auto spaceAfterWord = text.indexOf(spacePattern, cursorPosition);
    const auto wordEnd = spaceAfterWord == -1 ? text.size() : spaceAfterWord;
    currentWord.length = wordEnd - currentWord.begin;

    const auto currentWordSlice =
        textBeforeCursor.sliced(currentWord.begin, cursorPosition - currentWord.begin);

    // Completer will popup, when cursor placed after the bracket symbol (for e.g '{' is typed)
    // Completion prefix is set up as current word with bracket.
    completer->setCompletionPrefix(currentWordSlice.toString());
}

void Completer::Private::replaceCurrentWordWithCompleted(QString& text, const QString& completedText) const
{
    if (currentWord.length == 0)
    {
        text.insert(currentWord.begin, completedText);
    }
    else
    {
        text.remove(currentWord.begin, currentWord.length);
        text.insert(currentWord.begin, completedText);
    }
}


Completer::~Completer()
{
}

Completer::Completer(QAbstractListModel* model, QTextEdit* textEdit, QObject* parent):
    QObject(parent),
    d(new Private(this))
{
    d->completer = new QCompleter(model, this);
    d->initCompleterForTextEdit(textEdit);
}

Completer::Completer(const QStringList& words, QLineEdit* lineEdit, QObject* parent):
    QObject(parent),
    d(new Private(this))
{
    d->completer = new QCompleter(words, this);
    d->completer->setCaseSensitivity(Qt::CaseInsensitive);
    d->completer->setWidget(lineEdit);

    connect(d->completer,
        QOverload<const QString&>::of(&QCompleter::activated),
        this,
        [this, lineEdit](const QString& completedText)
        {
            auto currentText = lineEdit->text();
            d->replaceCurrentWordWithCompleted(currentText, completedText);

            QScopedValueRollback rollback{d->isCompletingInProgress, true};
            lineEdit->setText(currentText);
            lineEdit->setCursorPosition(d->currentWord.begin + completedText.size());
        });

    const auto triggerCompletion =
        [this, lineEdit]
        {
            if (d->isCompletingInProgress)
                return;

            d->updateCompleter(lineEdit->cursorPosition(), lineEdit->text());
            d->completer->complete(lineEdit->rect());
        };

    connect(lineEdit, &QLineEdit::textChanged, this, triggerCompletion);
    connect(lineEdit, &QLineEdit::cursorPositionChanged, this, triggerCompletion);
}

Completer::Completer(const QStringList& words, QTextEdit* textEdit, QObject* parent):
    QObject(parent),
    d(new Private(this))
{
    d->completer = new QCompleter(words, this);
    d->initCompleterForTextEdit(textEdit);
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
                d->completer->popup()->hide();
                emit d->completer->activated(d->completer->popup()->currentIndex().data().toString());
                return true;
            default:
                break;
        }
    }

    return QObject::eventFilter(obj, event);
}

} // namespace nx::vms::client::desktop::rules
