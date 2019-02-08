#include "search_line_edit.h"

#include <QtGui/QFocusEvent>
#include <QtWidgets/QCompleter>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QAction>

#include <ui/common/palette.h>
#include <ui/style/skin.h>
#include <utils/common/delayed.h>

#include <nx/utils/pending_operation.h>

namespace nx::vms::client::desktop {

SearchLineEdit::SearchLineEdit(QWidget* parent):
    QWidget(parent),
    m_lineEdit(new QLineEdit(this)),
    m_emitTextChanged(new utils::PendingOperation(
        [this]() { emit textChanged(m_lineEdit->text()); }, 0, this))
{
    setFocusPolicy(m_lineEdit->focusPolicy());
    setAttribute(Qt::WA_InputMethodEnabled);
    setSizePolicy(m_lineEdit->sizePolicy());
    setBackgroundRole(m_lineEdit->backgroundRole());
    setMouseTracking(true);
    setAcceptDrops(true);
    setAttribute(Qt::WA_MacShowFocusRect, true);
    QPalette p = m_lineEdit->palette();
    setPalette(p);

    // line edit
    m_lineEdit->setFocusProxy(this);
    m_lineEdit->setAttribute(Qt::WA_MacShowFocusRect, false);
    QPalette clearPalette = m_lineEdit->palette();
    clearPalette.setBrush(QPalette::Base, QBrush(Qt::transparent));
    m_lineEdit->setPalette(clearPalette);

    m_lineEdit->setPlaceholderText(tr("Search"));
    m_glassIcon = m_lineEdit->addAction(qnSkin->icon("theme/input_search.png"),
        QLineEdit::LeadingPosition);
    m_lineEdit->setClearButtonEnabled(true);

    m_emitTextChanged->setFlags(utils::PendingOperation::FireOnlyWhenIdle);

    connect(m_lineEdit, &QLineEdit::returnPressed, this, &SearchLineEdit::enterKeyPressed);

    connect(m_lineEdit, &QLineEdit::textChanged,
        m_emitTextChanged.data(), &utils::PendingOperation::requestOperation);

    QSizePolicy policy = sizePolicy();
    setSizePolicy(QSizePolicy::Preferred, policy.verticalPolicy());

    // Sets empty frame and invalidates geometry of control. Should be called outside constructor
    // to prevent wrong layout size recalculations.
    executeLater([this](){ m_lineEdit->setFrame(false); }, this);
}

SearchLineEdit::~SearchLineEdit()
{
}

void SearchLineEdit::initStyleOption(QStyleOptionFrame* option) const
{
    option->initFrom(this);
    option->rect = contentsRect();
    option->lineWidth = style()->pixelMetric(QStyle::PM_DefaultFrameWidth, option, this);
    option->midLineWidth = 0;
    option->state |= QStyle::State_Sunken;
    if (m_lineEdit->isReadOnly())
        option->state |= QStyle::State_ReadOnly;
#ifdef QT_KEYPAD_NAVIGATION
    if (hasEditFocus())
        option->state |= QStyle::State_HasEditFocus;
#endif
    option->features = QStyleOptionFrame::None;
}

QSize SearchLineEdit::sizeHint() const
{
    return m_lineEdit->sizeHint();
}

QString SearchLineEdit::text() const
{
    return m_lineEdit->text();
}

void SearchLineEdit::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    m_lineEdit->setGeometry(rect());
}

void SearchLineEdit::focusInEvent(QFocusEvent* event)
{
    m_lineEdit->event(event);
    m_lineEdit->selectAll();
    QWidget::focusInEvent(event);
}

void SearchLineEdit::focusOutEvent(QFocusEvent* event)
{
    m_lineEdit->event(event);

    if (m_lineEdit->completer())
    {
        connect(m_lineEdit->completer(), SIGNAL(activated(QString)),
            m_lineEdit, SLOT(setText(QString)));
        connect(m_lineEdit->completer(), SIGNAL(highlighted(QString)),
            m_lineEdit, SLOT(_q_completionHighlighted(QString)));
    }
    QWidget::focusOutEvent(event);
}

void SearchLineEdit::keyPressEvent(QKeyEvent* event)
{
    m_lineEdit->event(event);
    if ((event->key() == Qt::Key_Enter) || (event->key() == Qt::Key_Return))
        event->accept();
}

void SearchLineEdit::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::EnabledChange)
        emit enabledChanged();
}

bool SearchLineEdit::event(QEvent* event)
{
    if (event->type() == QEvent::ShortcutOverride)
    {
        QKeyEvent * const keyEvent = static_cast<QKeyEvent *>(event);
        const bool result = m_lineEdit->event(event);
        if (keyEvent->key() == Qt::Key_Escape)
        {
            emit escKeyPressed();
            keyEvent->accept();
        }
        return result;
    }

    return QWidget::event(event);
}

QVariant SearchLineEdit::inputMethodQuery(Qt::InputMethodQuery property) const
{
    return m_lineEdit->inputMethodQuery(property);
}

int SearchLineEdit::textChangedSignalFilterMs() const
{
    return m_emitTextChanged->intervalMs();
}

void SearchLineEdit::setTextChangedSignalFilterMs(int filterMs)
{
    m_emitTextChanged->setIntervalMs(filterMs);
}

void SearchLineEdit::clear()
{
    m_lineEdit->clear();
}

void SearchLineEdit::setGlassVisible(bool visible)
{
    m_glassIcon->setVisible(visible);
}

void SearchLineEdit::inputMethodEvent(QInputMethodEvent* event)
{
    m_lineEdit->event(event);
}

} // namespace nx::vms::client::desktop
