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

/** Search icon on the left hand side */
class QnSearchButton : public QAbstractButton {
public:
    QnSearchButton(QWidget *parent = 0): 
        QAbstractButton(parent)
    {
        setObjectName(QLatin1String("SearchButton"));
#ifndef QT_NO_CURSOR
        setCursor(Qt::ArrowCursor);
#endif //QT_NO_CURSOR
        setFocusPolicy(Qt::NoFocus);
    }

    void paintEvent(QPaintEvent *event) {
        Q_UNUSED(event);
        QPainterPath myPath;

        int radius = (height() / 5) * 2;
        QRect circle(height() / 3 - 1, height() / 4, radius, radius);
        myPath.addEllipse(circle);

        myPath.arcMoveTo(circle, 300);
        QPointF c = myPath.currentPosition();
        int diff = height() / 7;
        myPath.lineTo(qMin(width() - 2, (int)c.x() + diff), c.y() + diff);

        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.setPen(QPen(Qt::darkGray, 2));
        painter.drawPath(myPath);
        painter.end();
    }
};


QnClearButton::QnClearButton(QWidget *parent):
    QAbstractButton(parent)
{
#ifndef QT_NO_CURSOR
    setCursor(Qt::ArrowCursor);
#endif // QT_NO_CURSOR
    setToolTip(tr("Clear"));
    setVisible(false);
    setFocusPolicy(Qt::NoFocus);
}

void QnClearButton::paintEvent(QPaintEvent *event) {
    Q_UNUSED(event);
    QPainter painter(this);
    int height = this->height();

    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setBrush(isDown()
        ? palette().color(QPalette::Dark)
        : palette().color(QPalette::Mid));
    painter.setPen(painter.brush().color());
    int size = width();
    int offset = size / 5;
    int radius = size - offset * 2;
    painter.drawEllipse(offset, offset, radius, radius);

    painter.setPen(palette().color(QPalette::Base));
    int border = offset * 2;
    painter.drawLine(border, border, width() - border, height - border);
    painter.drawLine(border, height - border, width() - border, border);
}

void QnClearButton::textChanged(const QString &text) {
    setVisible(!text.isEmpty());
}


QnSearchLineEdit::QnSearchLineEdit(QWidget *parent) :
    QWidget(parent),
    m_lineEdit(new QLineEdit(this)),
    m_clearButton(new QnClearButton(this)),
    m_searchButton(new QnSearchButton(this))
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

    // clearButton
    connect(m_clearButton, SIGNAL(clicked()),
        m_lineEdit, SLOT(clear()));
    connect(m_lineEdit, SIGNAL(textChanged(QString)),
        m_clearButton, SLOT(textChanged(QString)));

    connect(m_lineEdit, SIGNAL(textChanged(QString)),
        this, SIGNAL(textChanged(QString)));
    m_lineEdit->setPlaceholderText(tr("Search"));

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

    int height = rect.height();
    int width = rect.width();

    int buttonSize = height;
    
    m_searchButton->setGeometry(rect.x(), rect.y(), buttonSize, buttonSize);
    m_lineEdit->setGeometry(m_searchButton->x() + buttonSize, 0, width - buttonSize*2, height);
    m_clearButton->setGeometry(width - buttonSize, 0, buttonSize, buttonSize);
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

void QnSearchLineEdit::focusInEvent(QFocusEvent *event) {
    m_lineEdit->event(event);
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

void QnSearchLineEdit::keyPressEvent(QKeyEvent *event) {
    m_lineEdit->event(event);
}

bool QnSearchLineEdit::event(QEvent *event) {
    if (event->type() == QEvent::ShortcutOverride)
        return m_lineEdit->event(event);
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
