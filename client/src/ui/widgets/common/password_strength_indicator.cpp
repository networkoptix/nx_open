#include "password_strength_indicator.h"

#include <ui/common/widget_anchor.h>
#include <ui/style/nx_style.h>

#include <utils/common/connective.h>
#include <utils/common/event_processors.h>

namespace
{
    /* Margins outside of indicator rectangle: */
    const QMargins kDefaultIndicatorMargins(6, 6, 6, 6);

    /* Rounding radius of indicator rectangle: */
    const qreal kDefaultRoundingRadius = 2.5;

    /* Text horizontal margins inside indicator rectangle: */
    const int kDefaultTextMargins = 8;

    /* Minimum width of indicator rectangle: */
    const int kDefaultMinimumWidth = 40;
}

/*
 * QnPasswordStrengthIndicatorPrivate
 */

class QnPasswordStrengthIndicatorPrivate: public ConnectiveBase
{
    QnPasswordStrengthIndicatorPrivate(QnPasswordStrengthIndicator* q, QLineEdit* lineEdit) :
        m_lineEdit(lineEdit),
        m_currentInformation(lineEdit->text()),
        m_roundingRadius(kDefaultRoundingRadius),
        m_textMargins(kDefaultTextMargins),
        m_anchor(new QnWidgetAnchor(q)),
        m_lineEditContentsMargins(lineEdit->contentsMargins()),
        m_eventSignalizer(new QnMultiEventSignalizer(q)),
        q_ptr(q)
    {
        m_eventSignalizer->addEventType(QEvent::ContentsRectChange);
        m_eventSignalizer->addEventType(QEvent::LayoutDirectionChange);
        m_lineEdit->installEventFilter(m_eventSignalizer);

        m_anchor->setEdges(anchorEdges());

        connect(m_eventSignalizer, &QnMultiEventSignalizer::activated, q,
            [this](QObject* object, QEvent* event)
            {
                Q_UNUSED(object);
                switch (event->type())
                {
                    case QEvent::ContentsRectChange:
                    {
                        m_lineEditContentsMargins = m_lineEdit->contentsMargins();
                        break;
                    }

                    case QEvent::LayoutDirectionChange:
                    {
                        m_anchor->setEdges(anchorEdges());
                        updateIndicator();
                        break;
                    }

                    default:
                        break;
                }
            });

        connect(m_lineEdit, &QLineEdit::textChanged, q,
            [this](const QString& text)
            {
                QnPasswordInformation newInformation(text);
                if (newInformation != m_currentInformation)
                {
                    m_currentInformation = newInformation;
                    updateIndicator();
                }
            });
    }

    void updateIndicator()
    {
        if (m_lineEdit.isNull())
            return;

        Q_Q(QnPasswordStrengthIndicator);
        QSize textSize = q->fontMetrics().size(Qt::TextSingleLine | Qt::TextHideMnemonic, m_currentInformation.text());
        q->resize(qMax(textSize.width() + m_textMargins, q->minimumWidth()), q->height());

        int extraContentMargin = q->width() + q->indicatorMargins().right() * 2;
        QMargins newMargins = m_lineEditContentsMargins;

        if (m_lineEdit->layoutDirection() == Qt::RightToLeft)
            newMargins.setLeft(qMax(newMargins.left(), extraContentMargin));
        else
            newMargins.setRight(qMax(newMargins.right(), extraContentMargin));

        QSignalBlocker blockEventSignals(m_eventSignalizer);
        m_lineEdit->setContentsMargins(newMargins);

        q->setToolTip(m_currentInformation.hint());

        q->update();
    }

    Qt::Edges anchorEdges() const
    {
        NX_ASSERT(m_lineEdit);
        Qt::LayoutDirection dir = m_lineEdit->layoutDirection();
        const int vertical = Qt::TopEdge | Qt::BottomEdge;
        const int horizontal = (dir == Qt::RightToLeft) ? Qt::LeftEdge : Qt::RightEdge;
        return Qt::Edges(horizontal | vertical);
    }

    QPointer<QLineEdit> m_lineEdit;
    QnPasswordInformation m_currentInformation;

    QnPasswordStrengthColors m_colors;
    qreal m_roundingRadius;
    int m_textMargins;

    QnWidgetAnchor* m_anchor;
    QMargins m_lineEditContentsMargins;
    QnMultiEventSignalizer* m_eventSignalizer;

    QnPasswordStrengthIndicator* q_ptr;
    Q_DECLARE_PUBLIC(QnPasswordStrengthIndicator)
};

/*
* QnPasswordStrengthIndicator
*/

QnPasswordStrengthIndicator::QnPasswordStrengthIndicator(QLineEdit* lineEdit) :
    base_type(lineEdit),
    d_ptr(new QnPasswordStrengthIndicatorPrivate(this, lineEdit))
{
    QFont textFont = font();
    textFont.setPixelSize(10);
    textFont.setBold(true);
    textFont.setCapitalization(QFont::AllUppercase);
    setFont(textFont);

    setCursor(Qt::ArrowCursor);

    setMinimumWidth(kDefaultMinimumWidth);
    setIndicatorMargins(kDefaultIndicatorMargins);
}

QnPasswordStrengthIndicator::~QnPasswordStrengthIndicator()
{
}

const QnPasswordInformation& QnPasswordStrengthIndicator::currentInformation() const
{
    Q_D(const QnPasswordStrengthIndicator);
    return d->m_currentInformation;
}

const QnPasswordStrengthColors& QnPasswordStrengthIndicator::colors() const
{
    Q_D(const QnPasswordStrengthIndicator);
    return d->m_colors;
}

void QnPasswordStrengthIndicator::setColors(const QnPasswordStrengthColors& colors)
{
    Q_D(QnPasswordStrengthIndicator);
    if (d->m_colors == colors)
        return;

    d->m_colors = colors;
    update();
}

const QMargins& QnPasswordStrengthIndicator::indicatorMargins() const
{
    Q_D(const QnPasswordStrengthIndicator);
    return d->m_anchor->margins();
}

void QnPasswordStrengthIndicator::setIndicatorMargins(const QMargins& margins)
{
    Q_D(QnPasswordStrengthIndicator);
    d->m_anchor->setMargins(margins);
    d->updateIndicator();
}

qreal QnPasswordStrengthIndicator::roundingRadius() const
{
    Q_D(const QnPasswordStrengthIndicator);
    return d->m_roundingRadius;
}

void QnPasswordStrengthIndicator::setRoundingRadius(qreal radius)
{
    Q_D(QnPasswordStrengthIndicator);
    if (d->m_roundingRadius == radius)
        return;

    d->m_roundingRadius = radius;
    update();
}

int QnPasswordStrengthIndicator::textMargins() const
{
    Q_D(const QnPasswordStrengthIndicator);
    return d->m_textMargins;
}

void QnPasswordStrengthIndicator::setTextMargins(int margins)
{
    Q_D(QnPasswordStrengthIndicator);
    if (d->m_textMargins == margins)
        return;

    d->m_textMargins = margins;
    d->updateIndicator();
}

void QnPasswordStrengthIndicator::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event)
    Q_D(const QnPasswordStrengthIndicator);

    QColor background;
    switch (d->m_currentInformation.acceptance())
    {
        case QnPasswordInformation::Good:
            background = d->m_colors.good;
            break;

        case QnPasswordInformation::Acceptable:
            background = d->m_colors.acceptable;
            break;

        default:
            background = d->m_colors.inacceptable;
            break;
    }

    QPainter painter(this);
    painter.setPen(Qt::NoPen);
    painter.setBrush(background);

    painter.setRenderHint(QPainter::Antialiasing);
    painter.drawRoundedRect(rect().adjusted(0.5, 0.5, -0.5, -0.5), d->m_roundingRadius, d->m_roundingRadius);

    painter.setPen(palette().color(QPalette::Shadow));
    painter.drawText(rect(), Qt::TextSingleLine | Qt::TextHideMnemonic | Qt::AlignCenter, d->m_currentInformation.text());
}
