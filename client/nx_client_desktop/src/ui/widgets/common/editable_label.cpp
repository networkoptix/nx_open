#include "editable_label.h"

#include <QtCore/QSignalBlocker>

#include <QtGui/QMouseEvent>

#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QStackedWidget>
#include <QtWidgets/QToolButton>

#include "ui/style/helper.h"
#include "ui/style/skin.h"

#include "ui/widgets/common/elided_label.h"

#include "utils/common/event_processors.h"

namespace {

static constexpr int kDefaultFontSizePixels = 24;
static constexpr int kDefaultFontWeight = QFont::DemiBold;

} // namespace


/* QnEditableLabelPrivate */

class QnEditableLabelPrivate: public QObject
{
    QnEditableLabel* q_ptr;
    Q_DECLARE_PUBLIC(QnEditableLabel);

public:
    QnEditableLabelPrivate(QnEditableLabel* q):
        QObject(q),
        q_ptr(q),
        pages(new QStackedWidget(q_ptr)),
        labelPage(new QWidget(pages)),
        editPage(new QWidget(pages)),
        label(new QnElidedLabel(labelPage)),
        button(new QToolButton(labelPage)),
        edit(new QLineEdit(editPage))
    {
        /* Main layout with stacked widget: */
        auto mainLayout = new QHBoxLayout(q_ptr);
        mainLayout->setContentsMargins(0, 0, 0, 0);
        mainLayout->addWidget(pages);

        /* Page with label and button: */
        auto labelPageLayout = new QHBoxLayout(labelPage);
        const int margin = style::Metrics::kStandardPadding;
        labelPageLayout->setContentsMargins(margin, 0, margin, 0);
        labelPageLayout->addWidget(label);
        labelPageLayout->addWidget(button);
        labelPageLayout->addStretch();
        pages->addWidget(labelPage);

        label->setElideMode(Qt::ElideRight);
        label->setForegroundRole(QPalette::Text);
        label->setProperty(style::Properties::kDontPolishFontProperty, true);
        label->setFont(q->font());

        updateCursor();

        installEventHandler(label, QEvent::MouseButtonPress, q,
            [this](QObject* sender, QEvent* event)
            {
                Q_UNUSED(sender);
                if (static_cast<QMouseEvent*>(event)->button() == Qt::LeftButton)
                    beginEditing();
            });

        // TODO: #common Button lacks default icon
        connect(button, &QToolButton::clicked, this, &QnEditableLabelPrivate::beginEditing);

        /* Page with line edit: */
        auto editPageLayout = new QHBoxLayout(editPage);
        editPageLayout->setContentsMargins(0, 0, 0, 0);
        editPageLayout->addWidget(edit);
        pages->addWidget(editPage);

        edit->setProperty(style::Properties::kDontPolishFontProperty, true);
        edit->setFont(q->font());

        connect(edit, &QLineEdit::textEdited, q, &QnEditableLabel::textChanging);
        connect(edit, &QLineEdit::editingFinished, this, &QnEditableLabelPrivate::commit);

        /* Event filter to handle global mouse clicks and intercept Esc and Enter keys: */
        qApp->installEventFilter(this);
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

        Q_Q(QnEditableLabel);
        if (editing())
        {
            /* If editing is on, emit textChanging: */
            emit q->textChanging(value);
        }
        else
        {
            /* If editing is off, emit textChanged: */
            label->setText(value);
            emit q->textChanged(value);
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

        pages->setCurrentWidget(editPage);
        edit->setFocus();

        Q_Q(QnEditableLabel);
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
        button->setFocus();

        Q_Q(QnEditableLabel);
        emit q->editingFinished();

        if (textChanged)
            emit q->textChanged(text());
    }

    void commit()
    {
        endEditing(true);
    }

    void revert()
    {
        endEditing(false);
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
        updateCursor();
    }

    void updateCursor()
    {
        if (readOnly())
            label->unsetCursor();
        else
            label->setCursor(Qt::IBeamCursor);
    }

    /* Event filter to handle global mouse clicks and intercept Esc and Enter keys: */
    virtual bool eventFilter(QObject* watched, QEvent* event) override
    {
        Q_UNUSED(watched);
        switch (event->type())
        {
            /*
             * Left button click outside of line edit during editing forces commit.
             */
            case QEvent::MouseButtonPress:
            {
                auto mouseEvent = static_cast<QMouseEvent*>(event);

                if (mouseEvent->button() != Qt::LeftButton || !editing())
                    return false;

                if (edit->rect().contains(edit->mapFromGlobal(mouseEvent->globalPos())))
                    return false;

                commit();
                return false; //< allow further processing
            }

            /*
             * Enter during editing forces commit, Esc forces rollback.
             * Such keypresses are eaten and not processed further.
             */
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
                        return true; //< block further processing

                    case Qt::Key_Escape:
                        event->accept();
                        revert();
                        return true; //< block further processing

                    default:
                        return false;
                }
            }

            default:
                return false;
        }
    }

public:
    QStackedWidget* pages;
    QWidget* labelPage;
    QWidget* editPage;
    QnElidedLabel* label;
    QToolButton* button;
    QLineEdit* edit;
    QnEditableLabel::Validator validator;
};


/* QnEditableLabel */

QnEditableLabel::QnEditableLabel(QWidget* parent):
    base_type(parent),
    d_ptr(new QnEditableLabelPrivate(this))
{
    QFont font;
    font.setPixelSize(kDefaultFontSizePixels);
    font.setWeight(kDefaultFontWeight);
    setFont(font);
    setButtonIcon(qnSkin->icon("system_settings/edit.png"));
    setValidator([this](QString& text) { return !text.isEmpty(); });
}

QnEditableLabel::~QnEditableLabel()
{
}

QString QnEditableLabel::text() const
{
    Q_D(const QnEditableLabel);
    return d->text();
}

void QnEditableLabel::setText(const QString& text)
{
    Q_D(QnEditableLabel);
    d->setText(text);
}

bool QnEditableLabel::editing() const
{
    Q_D(const QnEditableLabel);
    return d->editing();
}

void QnEditableLabel::setEditing(bool editing, bool applyChanges)
{
    Q_D(QnEditableLabel);
    if (editing)
        d->beginEditing();
    else
        d->endEditing(applyChanges);
}

QIcon QnEditableLabel::buttonIcon() const
{
    Q_D(const QnEditableLabel);
    return d->button->icon();
}

void QnEditableLabel::setButtonIcon(const QIcon& icon)
{
    Q_D(QnEditableLabel);
    d->button->setIcon(icon);

    auto size = QnSkin::maximumSize(icon);
    d->button->setIconSize(size);
    d->button->setFixedSize(size);
}

bool QnEditableLabel::readOnly() const
{
    Q_D(const QnEditableLabel);
    return d->readOnly();
}

void QnEditableLabel::setReadOnly(bool readOnly)
{
    Q_D(QnEditableLabel);
    d->setReadOnly(readOnly);
}

void QnEditableLabel::setValidator(Validator validator)
{
    Q_D(QnEditableLabel);
    d->validator = validator;
}

void QnEditableLabel::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::FontChange)
    {
        Q_D(QnEditableLabel);
        d->label->setFont(font());
        d->edit->setFont(font());
    }

    base_type::changeEvent(event);
}
