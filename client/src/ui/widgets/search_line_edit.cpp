#include "search_line_edit.h"

#include <QtCore/QEvent>
#include <QtCore/QMimeData>

#include <QtGui/QFocusEvent>
#include <QtGui/QDrag>
#include <QtGui/QPainter>

#include <QtWidgets/QApplication>
#include <QtWidgets/QCompleter>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QStyle>
#include <QtWidgets/QStyleOptionFrameV2>

#include <ui/style/skin.h>
#include <ui/common/palette.h>

namespace {
    /** Search icon on the left hand side */
    class QnSearchButton : public QAbstractButton {
    public:
        QnSearchButton(QWidget *parent = 0): 
            QAbstractButton(parent)
        {
            setObjectName(QLatin1String("QnSearchButton"));
#ifndef QT_NO_CURSOR
            setCursor(Qt::ArrowCursor);
#endif //QT_NO_CURSOR
            setFocusPolicy(Qt::NoFocus);
        }

        void paintEvent(QPaintEvent *event) {
            Q_UNUSED(event);

            int size = height(); //assuming height and width are equal
            QPainterPath &path = m_pathCacheBySize[size];
            if (path.isEmpty()) {
                int radius = (size / 5) * 2;
                QRect circle(size / 3 - 1, size / 4, radius, radius);
                path.addEllipse(circle);

                path.arcMoveTo(circle, 300);
                QPointF c = path.currentPosition();
                int diff = size / 7;
                path.lineTo(qMin(width() - 2, (int)c.x() + diff), c.y() + diff);
            }

            QPainter painter(this);
            painter.setRenderHint(QPainter::Antialiasing, true);
            painter.setPen(QPen(Qt::darkGray, 2));  //TODO: #GDM #Bookmarks should we customize this?
            painter.drawPath(path);                 //TODO: #GDM #Bookmarks should we cache pixmap?
            painter.end();
        }

    private:
        QMap<int, QPainterPath> m_pathCacheBySize;
    };

    QToolButton *createCloseButton(QnSearchLineEdit *parent)
    {
        QToolButton *result = new QToolButton(parent);
        result->setIcon(qnSkin->icon(lit("titlebar/close_tab.png")));
        result->setToolTip(QObject::tr("Close"));
        QObject::connect(result, &QAbstractButton::clicked, parent, &QnSearchLineEdit::escKeyPressed);

        return result;
    }

    QString getBookmarksCountText(int found
        , int overall)
    {
        return QString(lit("%1/%2")).arg(QString::number(found), QString::number(overall));
    }
}

QnSearchLineEdit::QnSearchLineEdit(QWidget *parent)
    : QWidget(parent)
    , m_lineEdit(new QLineEdit(this))
    , m_closeButton(createCloseButton(this))
    , m_searchButton(new QnSearchButton(this))

    , m_foundItems(0)
    , m_overallItems(0)

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
    m_lineEdit->setFrame(false);
    m_lineEdit->setFocusProxy(this);
    m_lineEdit->setAttribute(Qt::WA_MacShowFocusRect, false);
    QPalette clearPalette = m_lineEdit->palette();
    clearPalette.setBrush(QPalette::Base, QBrush(Qt::transparent));
    m_lineEdit->setPalette(clearPalette);
    m_lineEdit->setPlaceholderText(tr("Search"));
    connect(m_lineEdit, &QLineEdit::textChanged, this, &QnSearchLineEdit::textChanged);
    connect(m_lineEdit, &QLineEdit::returnPressed, this, &QnSearchLineEdit::enterKeyPressed);

    QSizePolicy policy = sizePolicy();
    setSizePolicy(QSizePolicy::Preferred, policy.verticalPolicy());
}

void QnSearchLineEdit::resizeEvent(QResizeEvent *event) {
    updateGeometries();
    QWidget::resizeEvent(event);
}

void QnSearchLineEdit::updateGeometries() {

    QStyleOptionFrameV2 panel;
    initStyleOption(&panel);
    QRect rect = style()->subElementRect(QStyle::SE_LineEditContents, &panel, this);

    const int height = rect.height();
    const int width = rect.width();
    const int buttonSize = height;
    
    const int closeButtonSize = height * 3 / 4;
    // left edge
    m_searchButton->setGeometry(rect.x(), rect.y(), buttonSize, buttonSize);

    // right edge
    m_closeButton->setGeometry(width - (buttonSize + closeButtonSize) / 2
        , rect.y() + (buttonSize - closeButtonSize) / 2 + 1, closeButtonSize, closeButtonSize);

    m_lineEdit->setGeometry(m_searchButton->x() + buttonSize, 0, m_closeButton->x() - buttonSize, height);
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

QSize QnSearchLineEdit::sizeHint() const {
    m_lineEdit->setFrame(true);
    QSize size = m_lineEdit->sizeHint();
    m_lineEdit->setFrame(false);
    return size;
}

void QnSearchLineEdit::focusInEvent(QFocusEvent *event) 
{
    m_lineEdit->event(event);
    m_lineEdit->selectAll();
    QWidget::focusInEvent(event);
}

void QnSearchLineEdit::focusOutEvent(QFocusEvent *event) {
    m_lineEdit->event(event);

    if (m_lineEdit->completer()) {
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
    {
        event->accept();
    }
}

void QnSearchLineEdit::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::EnabledChange)
    {
        m_closeButton->setVisible(isEnabled()); /// To update close button state when hiding/showing
        emit enabledChanged();
    }
}

bool QnSearchLineEdit::event(QEvent *event)
{
    if (event->type() == QEvent::ShortcutOverride)
    {
        QKeyEvent * const keyEvent = static_cast<QKeyEvent *>(event);
        const bool result = m_lineEdit->event(event);
        if (keyEvent->key() == Qt::Key_Escape)
        {
            escKeyPressed();
            keyEvent->accept();
        }
        return result;
    }

    return QWidget::event(event);
}

void QnSearchLineEdit::paintEvent(QPaintEvent *event) {
    Q_UNUSED(event)
    QPainter p(this);
    QStyleOptionFrameV2 panel;
    initStyleOption(&panel);
    style()->drawPrimitive(QStyle::PE_PanelLineEdit, &panel, &p, this);
}

QVariant QnSearchLineEdit::inputMethodQuery(Qt::InputMethodQuery property) const {
    return m_lineEdit->inputMethodQuery(property);
}

void QnSearchLineEdit::inputMethodEvent(QInputMethodEvent *e) {
    m_lineEdit->event(e);
}
