// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "editable_label.h"

#include <QtCore/QSignalBlocker>
#include <QtGui/QMouseEvent>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QStackedWidget>
#include <QtWidgets/QToolButton>

#include <nx/vms/client/core/skin/color_theme.h>
#include <nx/vms/client/core/skin/skin.h>
#include <nx/vms/client/desktop/skin/font_config.h>
#include <nx/vms/client/desktop/style/helper.h>
#include <ui/widgets/common/elided_label.h>
#include <utils/common/event_processors.h>

namespace {

static const QColor kLight4Color = "#E1E7EA";
static const nx::vms::client::core::SvgIconColorer::IconSubstitutions kIconSubstitutions = {
    {QIcon::Normal, {{kLight4Color, "light4"}}},
    {QIcon::Active, {{kLight4Color, "light5"}}},
    {QIcon::Selected, {{kLight4Color, "light3"}}},
};

} // namespace

namespace nx::vms::client::desktop {

//-------------------------------------------------------------------------------------------------
// EditableLabel::Private

class EditableLabel::Private: public QObject
{
    EditableLabel* const q = nullptr;

public:
    Private(EditableLabel* q):
        QObject(q),
        q(q),
        pages(new QStackedWidget(q)),
        labelPage(new QWidget(pages)),
        editPage(new QWidget(pages)),
        label(new QnElidedLabel(labelPage)),
        button(new QToolButton(labelPage)),
        edit(new QLineEdit(editPage))
    {
        // Main layout with stacked widget.
        auto mainLayout = new QHBoxLayout(q);
        mainLayout->setContentsMargins(0, 0, 0, 0);
        mainLayout->addWidget(pages);

        // Page with label and button.
        auto labelPageLayout = new QHBoxLayout(labelPage);
        const int sideMargin = style::Metrics::kStandardPadding;
        const int lineEditFrameWidth = 1;
        labelPageLayout->setContentsMargins(
            sideMargin, lineEditFrameWidth, sideMargin, lineEditFrameWidth);
        labelPageLayout->addWidget(label);
        labelPageLayout->addWidget(button);
        labelPageLayout->addStretch();
        pages->addWidget(labelPage);

        label->setElideMode(Qt::ElideRight);
        label->setForegroundRole(QPalette::Text);
        label->setProperty(style::Properties::kDontPolishFontProperty, true);
        label->setFont(q->font());
        button->setFocusPolicy(Qt::NoFocus);

        installEventHandler(label, QEvent::MouseButtonPress, q,
            [this](QObject*, QEvent* event)
            {
                if (static_cast<QMouseEvent*>(event)->button() == Qt::LeftButton)
                    beginEditing();
            });

        // TODO: #vkutin Button lacks default icon.
        connect(button, &QToolButton::clicked, this, &Private::beginEditing);

        // Page with line edit.
        auto editPageLayout = new QHBoxLayout(editPage);
        editPageLayout->setContentsMargins(0, 0, 0, 0);
        editPageLayout->addWidget(edit);
        pages->addWidget(editPage);

        edit->setProperty(style::Properties::kDontPolishFontProperty, true);
        edit->setFont(q->font());

        connect(edit, &QLineEdit::textEdited, q, &EditableLabel::textChanging);
        connect(edit, &QLineEdit::editingFinished, this, &Private::commit);

        // Event filter to handle global mouse clicks and intercept Esc and Enter keys.
        qApp->installEventFilter(this);
    }

    bool shouldDisplayPlaceholder() const
    {
        return !editing() && text().isEmpty() && !placeholderText.isEmpty();
    }

    void setDisplayPlaceholder(bool displayPlaceholder)
    {
        if (!NX_ASSERT(!editing()))
            return;

        label->setText(displayPlaceholder
            ? placeholderText
            : text());

        label->setForegroundRole(displayPlaceholder
            ? QPalette::PlaceholderText
            : QPalette::Text);
    }

    QString text() const
    {
        return edit->text();
    }

    void setText(const QString& value)
    {
        if (text() == value)
            return;

        edit->setText(value);

        if (editing())
        {
            // If editing is on, emit textChanging.
            emit q->textChanging(value);
        }
        else
        {
            // If editing is off, emit textChanged.
            label->setText(value);
            emit q->textChanged(value);
            setDisplayPlaceholder(shouldDisplayPlaceholder());
        }
    }

    bool editing() const
    {
        return pages->currentWidget() == editPage;
    }

    void beginEditing()
    {
        if (editing() || readOnly())
            return;

        setDisplayPlaceholder(false);

        pages->setCurrentWidget(editPage);
        edit->setFocus();

        emit q->editingStarted();
    }

    void endEditing(bool applyChanges)
    {
        if (!editing())
            return;

        bool textChanged;
        if (applyChanges && validate())
        {
            textChanged = label->text() != edit->text();
            label->setText(edit->text());
        }
        else
        {
            textChanged = false;
            edit->setText(label->text());
        }

        pages->setCurrentWidget(labelPage);
        edit->clearFocus();

        emit q->editingFinished();

        if (textChanged)
            emit q->textChanged(text());
    }

    void commit()
    {
        endEditing(true);
        setDisplayPlaceholder(shouldDisplayPlaceholder());
    }

    void revert()
    {
        endEditing(false);
        setDisplayPlaceholder(shouldDisplayPlaceholder());
    }

    bool validate()
    {
        if (!validator)
            return true;

        QString input = edit->text();
        if (!validator(input))
            return false;

        edit->setText(input);
        return true;
    }

    bool readOnly() const
    {
        return button->isHidden();
    }

    void setReadOnly(bool readOnly)
    {
        if (readOnly == this->readOnly())
            return;

        if (readOnly && editing())
            revert();

        button->setHidden(readOnly);
    }

    // Event filter to handle global mouse clicks and intercept Esc and Enter keys.
    virtual bool eventFilter(QObject* watched, QEvent* event) override
    {
        switch (event->type())
        {
            // Left button click outside of line edit during editing forces commit.
            case QEvent::MouseButtonPress:
            {
                // Handle only widgets (now windows), skip the edit itself and its context menu.
                const auto watchedWidget = qobject_cast<const QWidget*>(watched);
                if (!watchedWidget || watchedWidget == edit || watchedWidget->parent() == edit)
                    return false;

                auto mouseEvent = static_cast<QMouseEvent*>(event);

                if (mouseEvent->button() != Qt::LeftButton || !editing())
                    return false;

                // This check is to prevent finishing edit right after start by clicking the label.
                if (watchedWidget->window() == edit->window()
                    && edit->rect().contains(edit->mapFromGlobal(mouseEvent->globalPos())))
                {
                    return false;
                }

                commit();
                return false; //< Allow further processing.
            }

            // Enter during editing forces commit, Esc forces rollback.
            // Such keypresses are eaten and not processed further.
            case QEvent::KeyPress:
            {
                if (!edit->hasFocus())
                    return false;

                NX_ASSERT(event->type() == QEvent::KeyPress);
                auto keyEvent = static_cast<QKeyEvent*>(event);

                switch (keyEvent->key())
                {
                    case Qt::Key_Enter:
                    case Qt::Key_Return:
                        event->accept();
                        commit();
                        return true; //< Block further processing.

                    case Qt::Key_Escape:
                        event->accept();
                        revert();
                        return true; //< Block further processing.

                    default:
                        return false;
                }
            }

            default:
                return false;
        }
    }

    QStackedWidget* const pages = nullptr;
    QWidget* const labelPage = nullptr;
    QWidget* const editPage = nullptr;
    QnElidedLabel* const label = nullptr;
    QToolButton* const button = nullptr;
    QLineEdit* const edit = nullptr;
    EditableLabel::Validator validator;
    QString placeholderText;
};

//-------------------------------------------------------------------------------------------------
// EditableLabel

EditableLabel::EditableLabel(QWidget* parent):
    base_type(parent),
    d(new Private(this))
{
    setFont(fontConfig()->font("editableLabel"));
    setButtonIcon(qnSkin->icon("system_settings/edit_28.svg", kIconSubstitutions));
    setValidator([](const QString& text) { return !text.isEmpty(); });
}

EditableLabel::~EditableLabel()
{
}

QString EditableLabel::text() const
{
    return d->text();
}

void EditableLabel::setText(const QString& text)
{
    d->setText(text);
}

QString EditableLabel::placeholderText()
{
    return d->placeholderText;
}

void EditableLabel::setPlaceholderText(const QString& placeholderText)
{
    d->placeholderText = placeholderText;
    d->setDisplayPlaceholder(d->shouldDisplayPlaceholder());
}

bool EditableLabel::editing() const
{
    return d->editing();
}

void EditableLabel::setEditing(bool editing, bool applyChanges)
{
    if (editing)
        d->beginEditing();
    else
        d->endEditing(applyChanges);
}

QIcon EditableLabel::buttonIcon() const
{
    return d->button->icon();
}

void EditableLabel::setButtonIcon(const QIcon& icon)
{
    d->button->setIcon(icon);

    auto size = core::Skin::maximumSize(icon);
    d->button->setIconSize(size);
    d->button->setFixedSize(size);
}

bool EditableLabel::readOnly() const
{
    return d->readOnly();
}

void EditableLabel::setReadOnly(bool readOnly)
{
    d->setReadOnly(readOnly);
}

void EditableLabel::setValidator(Validator validator)
{
    d->validator = validator;
}

} // namespace nx::vms::client::desktop
