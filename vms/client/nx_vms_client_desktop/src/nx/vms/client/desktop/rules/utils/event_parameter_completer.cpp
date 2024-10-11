// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "event_parameter_completer.h"

#include <QListView>
#include <QScrollBar>

#include <QtCore/QScopedValueRollback>
#include <QtGui/QKeyEvent>
#include <QtWidgets/QAbstractItemView>

#include <nx/utils/log/assert.h>
#include <nx/vms/client/core/skin/color_theme.h>
#include <nx/vms/client/desktop/rules/model_view/event_parameters_model.h>
#include <nx/vms/rules/utils/event_parameter_helper.h>
#include <ui/common/palette.h>

#include "event_parameters_dropdown_delegate.h"
#include "separator.h"

namespace nx::vms::client::desktop::rules {

using namespace nx::vms::rules::utils;

bool isSelectable(const QModelIndex& index)
{
    return !index.data(Qt::AccessibleDescriptionRole)
        .canConvert<Separator>();
}

struct EventParameterCompleter::Private: public QObject
{
    QCompleter* completer{nullptr};
    EventParameterCompleter* q;
    // Same pointer as completer->model(). Kept to avoid frequent dynamic cast.
    EventParametersModel* completerModel {nullptr};
    // Same pointer as completer->widget(). Kept to avoid frequent dynamic cast.
    QTextEdit* textEdit{nullptr};

    struct CurrentWord
    {
        int begin{0};
        int length{0};
        QString wordBeforeCursor;
    };

    CurrentWord currentReplaceableWord;
    bool isCompletingInProgress{false};

    Private(EventParameterCompleter* parent): q(parent){};
    void updateCompleter();
    void replaceCurrentWordWithCompleted(QString& text, const QString& completedText) const;
    void initCompleter(QTextEdit* inputTextEdit);
    void initCurrentWord(int cursorPosition, const QString& text);
    void initPopup(QAbstractItemView* popup);
    void triggerCompleter();
    void completeText(const QString& textToComplete, bool moveCursorBeforeEndingSymbol);
    bool isSameAsCurrentWord(QString replacement);
    void onCompleteActivated(const QString& textFromCompleter);
};

void EventParameterCompleter::Private::initCurrentWord(int cursorPosition, const QString& text)
{
    const QStringView textBeforeCursor{text.data(), cursorPosition};
    const auto startOfEventParameter =
        EventParameterHelper::getLatestEventParameterPos(textBeforeCursor, cursorPosition - 1);

    if (startOfEventParameter == -1)
    {
        // It's regular text. Insert text from completer without replacement.
        currentReplaceableWord = CurrentWord{.begin = 0};
        return;
    }

    currentReplaceableWord.begin = startOfEventParameter;

    // Find closing symbol, since event parameters can contain spaces,
    // e.g., {event.attributes.License Plate}.
    const int nearestEndOfEventParameter =
        text.indexOf(EventParameterHelper::endCharOfEventParameter(), cursorPosition);

    const int startOfNextEventParameter =
        text.indexOf(EventParameterHelper::startCharOfEventParameter(), cursorPosition);

    const bool isBracketBeforeOtherBracket =
        startOfNextEventParameter != -1 && startOfNextEventParameter < nearestEndOfEventParameter;
    const bool isReplacingCompleteEventParameter =
        !isBracketBeforeOtherBracket && nearestEndOfEventParameter != -1;

    // Set the word length, based on whether we are replacing a complete event parameter or not.
    currentReplaceableWord.length = isReplacingCompleteEventParameter
        ? nearestEndOfEventParameter - currentReplaceableWord.begin + 1
        // Unfinished event parameter, replace until the cursor.
        : cursorPosition - currentReplaceableWord.begin;

    currentReplaceableWord.wordBeforeCursor =
        textBeforeCursor
            .sliced(currentReplaceableWord.begin, cursorPosition - currentReplaceableWord.begin)
            .toString();
}

void EventParameterCompleter::Private::onCompleteActivated(const QString& textFromCompleter)
{
    if (textFromCompleter.isEmpty())
        return;

    const bool shouldTriggerCompleter = completerModel->isSubGroupName(textFromCompleter)
        && !isSameAsCurrentWord(textFromCompleter);

    completeText(textFromCompleter, shouldTriggerCompleter);
    if (!shouldTriggerCompleter)
        completerModel->resetExpandedGroup();
}

bool EventParameterCompleter::Private::isSameAsCurrentWord(QString replacement)
{
    return currentReplaceableWord.wordBeforeCursor == replacement.removeLast();
}

void EventParameterCompleter::Private::completeText(const QString& completedText, bool moveCursorBeforeEndingSymbol)
{
    if (!textEdit)
        return;

    auto currentText = textEdit->toPlainText();

    replaceCurrentWordWithCompleted(currentText, completedText);

    QScopedValueRollback rollback{isCompletingInProgress, true};

    if (currentText != textEdit->toPlainText())
        textEdit->setText(currentText);

    int newCursorPos = currentReplaceableWord.begin + completedText.size();
    if (moveCursorBeforeEndingSymbol)
        --newCursorPos; //< Cursor is moved before '}' when completed text is subgroup.
    auto cursor = textEdit->textCursor();
    cursor.setPosition(newCursorPos);
    textEdit->setTextCursor(cursor);
}

void EventParameterCompleter::Private::triggerCompleter()
{
    if (isCompletingInProgress || textEdit == nullptr || !textEdit->hasFocus())
        return;

    const auto cursor = textEdit->textCursor();
    if (cursor.hasSelection())
        return; //< To avoid triggering on selection.

    initCurrentWord(cursor.position(), textEdit->toPlainText());
    updateCompleter();

    // Completer must popup, only when cursor placed after the bracket symbol.
    if (!EventParameterHelper::isIncompleteEventParameter(currentReplaceableWord.wordBeforeCursor))
    {
        completer->popup()->hide();
        return;
    }

    const auto cursorRect =
        textEdit->cursorRect().adjusted(0, 0, 0, textEdit->fontMetrics().descent());
    const auto textEditRect = textEdit->rect();
    completer->complete(QRect{
        QPoint{textEditRect.left(), cursorRect.top()},
        QPoint{textEditRect.right(), cursorRect.bottom()}});
}


void EventParameterCompleter::Private::initPopup(QAbstractItemView* popup)
{
    popup->installEventFilter(q);
    popup->setItemDelegate(new EventParametersDropDownDelegate(q));
    popup->setSelectionMode(QAbstractItemView::SingleSelection);
    setPaletteColor(popup, QPalette::Base, core::colorTheme()->color("dark13"));
    // Color of highlight and scroll.
    setPaletteColor(popup, QPalette::Midlight, core::colorTheme()->color("dark16"));
}

void EventParameterCompleter::Private::initCompleter(QTextEdit* inputTextEdit)
{
    completer->setCaseSensitivity(Qt::CaseInsensitive);
    completer->setWidget(inputTextEdit);
    textEdit = inputTextEdit;
    initPopup(completer->popup());

    connect(inputTextEdit, &QTextEdit::textChanged, this, &Private::triggerCompleter);
    connect(inputTextEdit, &QTextEdit::cursorPositionChanged, this, &Private::triggerCompleter);
}

void EventParameterCompleter::Private::updateCompleter()
{
    // Completion filter is set up as current word with bracket.
    completerModel->expandGroup(currentReplaceableWord.wordBeforeCursor);
    completerModel->filter(currentReplaceableWord.wordBeforeCursor);
}

void EventParameterCompleter::Private::replaceCurrentWordWithCompleted(
    QString& text, const QString& completedText) const
{
    if (currentReplaceableWord.length == 0)
    {
        text.insert(currentReplaceableWord.begin, completedText);
    }
    else
    {
        text.remove(currentReplaceableWord.begin, currentReplaceableWord.length);
        text.insert(currentReplaceableWord.begin, completedText);
    }
}


EventParameterCompleter::~EventParameterCompleter()
{
}

void EventParameterCompleter::setModel(EventParametersModel* model)
{
    if (!NX_ASSERT(model, "Null completer model is not allowed"))
        return;

    d->completerModel = model;
    model->setParent(d->completer);
    // Previous model is deleted, when settting a new one.
    d->completer->setModel(d->completerModel);
}

bool EventParameterCompleter::containsElement(const QString& element)
{
    return d->completerModel->isCorrectParameter(element);
}

EventParameterCompleter::EventParameterCompleter(EventParametersModel* model, QTextEdit* textEdit,
    QObject* parent):
    QObject(parent),
    d(new Private(this))
{
    d->completer = new QCompleter(this);
    setModel(model);
    d->initCompleter(textEdit);

    connect(d->completer,
        QOverload<const QString&>::of(&QCompleter::activated),
        d.get(),
        &Private::onCompleteActivated);
}

bool EventParameterCompleter::eventFilter(QObject* obj, QEvent* event)
{
    const auto scroll = [&](const int increment)
    {
        auto nextIndex = d->completer->currentIndex().siblingAtRow(d->completer->popup()->currentIndex().row() + increment);
        if (!nextIndex.isValid())
            return true;

        if(!isSelectable(nextIndex))
            nextIndex = nextIndex.siblingAtRow(nextIndex.row() + increment);

        d->completer->popup()->setCurrentIndex(nextIndex);
        d->completer->popup()->scrollTo(nextIndex);
        d->completer->popup()->update();
        return true;
    };

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

            case Qt::Key_Down:
                scroll(1);
                return true;

            case Qt::Key_Up:
                scroll(-1);
                return true;

            default:
                break;
        }
    }

    return QObject::eventFilter(obj, event);
}

} // namespace nx::vms::client::desktop::rules
