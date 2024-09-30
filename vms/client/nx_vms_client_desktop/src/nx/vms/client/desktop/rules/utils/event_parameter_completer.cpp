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
    void completeText(const QString& textToComplete);
    bool isSameAsCurrentWord(const QString& replacement);
    void onCompleteActivated(const QString& textFromCompleter);
};

void EventParameterCompleter::Private::initCurrentWord(int cursorPosition, const QString& text)
{
    const QStringView textBeforeCursor{text.data(), cursorPosition};
    const auto startOfSubstitution =
        EventParameterHelper::getLatestEventParameterPos(textBeforeCursor, cursorPosition - 1);
    currentReplaceableWord.begin = startOfSubstitution == -1 ? 0 : startOfSubstitution;

    int wordEnd = text.size();
    int wordLength = 0;
    if (startOfSubstitution == -1)
    {
        // It's regular word, searching for first space.
        static const QRegularExpression spacePattern{"\\s", QRegularExpression::CaseInsensitiveOption};
        const auto spaceAfterWord = text.indexOf(spacePattern, cursorPosition);
        wordEnd = spaceAfterWord == -1 ? text.size() : spaceAfterWord;
        wordLength = wordEnd - currentReplaceableWord.begin;
    }
    else //< It's subsititution, searching for substitution symbols.
    {
        // Searching for closing symbol,
        // since substitution can contain spaces, for example {event.attributes.License Plate}.
        const int nearestEndOfEventParameter =
            text.indexOf(EventParameterHelper::endCharOfEventParameter(), cursorPosition);

        // To handle case, when new substitution is put before previous, need to search for
        // start of next parameter.
        const int startOfNextSubstitution =
            text.indexOf(EventParameterHelper::startCharOfEventParameter(), cursorPosition);

        if (startOfNextSubstitution != -1 && startOfNextSubstitution < nearestEndOfEventParameter)
        {
            wordEnd = startOfNextSubstitution;
            wordLength = wordEnd - currentReplaceableWord.begin;
        }
        else
        {
            wordEnd = nearestEndOfEventParameter == -1 ? text.size() : nearestEndOfEventParameter;
            wordLength = wordEnd - currentReplaceableWord.begin + 1;
        }
    }

    currentReplaceableWord.length = wordLength;
    currentReplaceableWord.wordBeforeCursor =
        textBeforeCursor
            .sliced(currentReplaceableWord.begin, cursorPosition - currentReplaceableWord.begin)
            .toString();
}

void EventParameterCompleter::Private::onCompleteActivated(const QString& textFromCompleter)
{
    if (textFromCompleter.isEmpty())
        return;

    QString textToComplete = textFromCompleter;
    const bool isSubGroupName = completerModel->isSubGroupName(textToComplete);
    bool needToReshowPopup = isSubGroupName;
    if (isSubGroupName)
    {
        const auto lastChar = textToComplete.back();
        if (!NX_ASSERT(EventParameterHelper::isEndOfEventParameter(lastChar)))
            return;

        textToComplete.removeLast(); // Remove closing bracket, to be able fill subgroups.
        if (isSameAsCurrentWord(textToComplete))
        {
            // The current name of subgroup is already a desired replacement.
            // Finish typing of paramter.
            needToReshowPopup = false;
            textToComplete += lastChar;
        }
    }

    completeText(textToComplete);

    if (isSubGroupName && needToReshowPopup)
        triggerCompleter();
    else
        completerModel->resetExpandedGroup();
}

bool EventParameterCompleter::Private::isSameAsCurrentWord(const QString& replacement)
{
    return currentReplaceableWord.wordBeforeCursor == replacement;
}

void EventParameterCompleter::Private::completeText(const QString& completedText)
{
    if (!textEdit)
        return;

    auto currentText = textEdit->toPlainText();

    replaceCurrentWordWithCompleted(currentText, completedText);

    if (currentText == textEdit->toPlainText())
        return;

    QScopedValueRollback rollback{isCompletingInProgress, true};
    textEdit->setText(currentText);

    auto cursor = textEdit->textCursor();
    cursor.setPosition(currentReplaceableWord.begin + completedText.size());
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

    // Completer will popup, when cursor placed after the bracket symbol (for e.g '{' is typed)
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
