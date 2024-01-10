// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "completer.h"

#include <QtCore/QScopedValueRollback>
#include <QtGui/QKeyEvent>
#include <QtWidgets/QAbstractItemView>

#include <nx/utils/log/assert.h>
#include <nx/vms/client/core/skin/color_theme.h>
#include <nx/vms/client/desktop/rules/model_view/event_parameters_model.h>
#include <nx/vms/rules/utils/event_parameter_helper.h>
#include <ui/common/palette.h>

#include "event_parameters_dropdown_delegate.h"

namespace nx::vms::client::desktop::rules {

using namespace nx::vms::rules::utils;

struct Completer::Private: public QObject
{
    QCompleter* completer{nullptr};
    Completer* q;
    // Same pointer as completer->model(). Kept to avoid frequent dynamic cast.
    EventParametersModel* completerModel {nullptr};
    // Same pointer as completer->widget(). Kept to avoid frequent dynamic cast.
    QTextEdit* textEdit{nullptr};

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
    void initCompleterForTextEdit(QTextEdit* inputTextEdit);
    void initPopup(QAbstractItemView* popup);
    void triggerCompleterForTextEdit();
    void completeTextForTextEdit(const QString& textToComplete);
    bool isSameAsCurrentWord(const QString& currentText, const QString& replacement);
    void onCompleteActivatedForModel(const QString& textFromCompleter);
};

void Completer::Private::onCompleteActivatedForModel(const QString& textFromCompleter)
{
    if (!completerModel || textFromCompleter.isEmpty())
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
        if (isSameAsCurrentWord(textEdit->toPlainText(), textToComplete))
        {
            // The current name of subgroup is already a desired replacement.
            // Finish typing of paramter.
            needToReshowPopup = false;
            textToComplete += lastChar;
        }
    }

    completeTextForTextEdit(textToComplete);

    if (isSubGroupName && needToReshowPopup)
        triggerCompleterForTextEdit();
    else
        completerModel->resetExpandedGroup();
}

bool Completer::Private::isSameAsCurrentWord(
    const QString& currentText, const QString& replacement)
{
    if (currentWord.length == 0)
        return false;

    const QString word = currentText.sliced(currentWord.begin, currentWord.length);
    return word == replacement;
}

void Completer::Private::completeTextForTextEdit(const QString& completedText)
{
    if (!textEdit)
        return;

    auto currentText = textEdit->toPlainText();

    replaceCurrentWordWithCompleted(currentText, completedText);

    QScopedValueRollback rollback{isCompletingInProgress, true};
    textEdit->setText(currentText);

    auto cursor = textEdit->textCursor();
    cursor.setPosition(currentWord.begin + completedText.size());
    textEdit->setTextCursor(cursor);
}

void Completer::Private::triggerCompleterForTextEdit()
{
    if (isCompletingInProgress || textEdit == nullptr)
        return;

    updateCompleter(textEdit->textCursor().position(), textEdit->toPlainText());

    const auto cursorRect =
        textEdit->cursorRect().adjusted(0, 0, 0, textEdit->fontMetrics().descent());
    const auto textEditRect = textEdit->rect();
    completer->complete(QRect{
        QPoint{textEditRect.left(), cursorRect.top()},
        QPoint{textEditRect.right(), cursorRect.bottom()}});
}


void Completer::Private::initPopup(QAbstractItemView* popup)
{
    popup->installEventFilter(q);
    popup->setItemDelegate(new EventParametersDropDownDelegate(q));
    setPaletteColor(popup, QPalette::Base, core::colorTheme()->color("dark13"));
    // Color of highlight and scroll.
    setPaletteColor(popup, QPalette::Midlight, core::colorTheme()->color("dark16"));
}

void Completer::Private::initCompleterForTextEdit(QTextEdit* inputTextEdit)
{
    completer->setCaseSensitivity(Qt::CaseInsensitive);
    completer->setWidget(inputTextEdit);
    textEdit = inputTextEdit;
    initPopup(completer->popup());

    connect(inputTextEdit,
        &QTextEdit::textChanged,
        this,
        &Completer::Private::triggerCompleterForTextEdit);
    connect(inputTextEdit,
        &QTextEdit::cursorPositionChanged,
        this,
        &Completer::Private::triggerCompleterForTextEdit);
}

void Completer::Private::updateCompleter(int cursorPosition, const QString& text)
{
    static const QRegularExpression spacePattern{"\\s", QRegularExpression::CaseInsensitiveOption};

    const QStringView textBeforeCursor{text.data(), cursorPosition};
    const auto startOfSubstitution =
        EventParameterHelper::getLatestEventParameterPos(textBeforeCursor.toString());
    currentWord.begin = startOfSubstitution == -1 ? 0 : startOfSubstitution;

    const auto spaceAfterWord = text.indexOf(spacePattern, cursorPosition);
    const auto wordEnd = spaceAfterWord == -1 ? text.size() : spaceAfterWord;
    currentWord.length = wordEnd - currentWord.begin;

    const auto currentWordSlice =
        textBeforeCursor.sliced(currentWord.begin, cursorPosition - currentWord.begin);

    if (completerModel)
        completerModel->expandGroup(currentWordSlice.toString());
    // Completer will popup, when cursor placed after the bracket symbol (for e.g '{' is typed)
    // Completion prefix is set up as current word with bracket.
    completer->setCompletionPrefix(currentWordSlice.toString()); // TODO: #vbutkevich fix grouping
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

void Completer::setModel(EventParametersModel* model)
{
    if (model == nullptr)
        return;

    d->completerModel = model;
    model->setParent(d->completer);
    d->completer->setModel(d->completerModel);
}

bool Completer::containsElement(const QString& element)
{
    if (d->completerModel)
        return d->completerModel->isCorrectParameter(element);

    return true; // TODO: #vbutkevich need to add solution for other types of models
}

Completer::Completer(EventParametersModel* model, QTextEdit* textEdit, QObject* parent):
    QObject(parent),
    d(new Private(this))
{
    d->completer = new QCompleter(this);
    setModel(model);
    d->initCompleterForTextEdit(textEdit);

    connect(d->completer,
        QOverload<const QString&>::of(&QCompleter::activated),
        d.get(),
        &Completer::Private::onCompleteActivatedForModel);
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

    connect(d->completer,
        QOverload<const QString&>::of(&QCompleter::activated),
        d.get(),
        &Completer::Private::completeTextForTextEdit);
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
