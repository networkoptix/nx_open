#include "search_line_edit.h"

#include <QtCore/QEvent>
#include <QtCore/QMimeData>
#include <QtCore/QTimer>

#include <QtGui/QFocusEvent>
#include <QtGui/QDrag>
#include <QtGui/QPainter>

#include <QtWidgets/QApplication>
#include <QtWidgets/QCompleter>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QStyle>

#include <ui/common/palette.h>
#include <ui/style/skin.h>

#include <utils/common/delayed.h>

QnSearchLineEdit::QnSearchLineEdit(QWidget *parent)
    : QWidget(parent)
    , m_lineEdit(new QLineEdit(this))
    , m_textChangedSignalFilterMs(0)
    , m_filterTimer()
{
    setAttribute(Qt::WA_InputMethodEnabled);
    setFocusPolicy(m_lineEdit->focusPolicy());
    setSizePolicy(m_lineEdit->sizePolicy());
    setBackgroundRole(m_lineEdit->backgroundRole());
    QPalette p = m_lineEdit->palette();
    setPalette(p);

    setMouseTracking(true);
    setAcceptDrops(true);
    setAttribute(Qt::WA_MacShowFocusRect, true);


    // line edit
    m_lineEdit->setFrame(false);
    m_lineEdit->setFocusProxy(this);
    m_lineEdit->setAttribute(Qt::WA_MacShowFocusRect, false);
    QPalette clearPalette = m_lineEdit->palette();
    clearPalette.setBrush(QPalette::Base, QBrush(Qt::transparent));
    m_lineEdit->setPalette(clearPalette);

//    m_lineEdit->setPlaceholderText(tr("Search"));
//    m_lineEdit->addAction(qnSkin->icon("theme/input_search.png"), QLineEdit::LeadingPosition);
    m_lineEdit->setClearButtonEnabled(true);

    connect(m_lineEdit, &QLineEdit::returnPressed, this, &QnSearchLineEdit::enterKeyPressed);

    connect(m_lineEdit, &QLineEdit::textChanged, this
        , [this](const QString &text)
    {
        const auto emitTextChanged = [this, text]()
        {
            emit textChanged(text);
            m_filterTimer.reset();
        };

        if (m_textChangedSignalFilterMs <= 0)
        {
            emitTextChanged();
            return;
        }

        m_filterTimer.reset(executeDelayedParented(
            emitTextChanged, m_textChangedSignalFilterMs, this));
    });

    QSizePolicy policy = sizePolicy();
    setSizePolicy(QSizePolicy::Preferred, policy.verticalPolicy());
}

void QnSearchLineEdit::initStyleOption(QStyleOptionFrameV2 *option) const
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
    option->features = QStyleOptionFrameV2::None;
}

QSize QnSearchLineEdit::sizeHint() const
{
    m_lineEdit->setFrame(true);
    QSize size = m_lineEdit->sizeHint();
    m_lineEdit->setFrame(false);
    return size;
}

QString QnSearchLineEdit::text() const
{
    return m_lineEdit->text();
}

void QnSearchLineEdit::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    m_lineEdit->setGeometry(rect());
}

void QnSearchLineEdit::focusInEvent(QFocusEvent *event)
{
    m_lineEdit->event(event);
    m_lineEdit->selectAll();
    QWidget::focusInEvent(event);
}

void QnSearchLineEdit::focusOutEvent(QFocusEvent *event)
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

void QnSearchLineEdit::keyPressEvent(QKeyEvent *event)
{
    m_lineEdit->event(event);
    if ((event->key() == Qt::Key_Enter) || (event->key() == Qt::Key_Return))
        event->accept();
}

void QnSearchLineEdit::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::EnabledChange)
        emit enabledChanged();
}

bool QnSearchLineEdit::event(QEvent *event)
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

QVariant QnSearchLineEdit::inputMethodQuery(Qt::InputMethodQuery property) const
{
    return m_lineEdit->inputMethodQuery(property);
}

int QnSearchLineEdit::textChangedSignalFilterMs() const
{
    return m_textChangedSignalFilterMs;
}

void QnSearchLineEdit::setTextChangedSignalFilterMs(int filterMs)
{
    m_textChangedSignalFilterMs = filterMs;
}

void QnSearchLineEdit::clear()
{
    m_lineEdit->clear();
}

void QnSearchLineEdit::inputMethodEvent(QInputMethodEvent *e)
{
    m_lineEdit->event(e);
}
