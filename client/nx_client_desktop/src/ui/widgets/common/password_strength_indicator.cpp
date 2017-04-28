#include "password_strength_indicator.h"

#include <QtGui/QPainter>

#include <QtWidgets/QLineEdit>

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
        lineEdit(lineEdit),
        currentInformation(lineEdit->text()),
        roundingRadius(kDefaultRoundingRadius),
        textMargins(kDefaultTextMargins),
        anchor(new QnWidgetAnchor(q)),
        lineEditContentsMargins(lineEdit->contentsMargins()),
        eventSignalizer(nullptr),
        q_ptr(q)
    {
        eventSignalizer = installEventHandler(lineEdit,
            { QEvent::ContentsRectChange, QEvent::LayoutDirectionChange }, q,
            [this](QObject* object, QEvent* event)
            {
                Q_UNUSED(object);
                switch (event->type())
                {
                    case QEvent::ContentsRectChange:
                    {
                        lineEditContentsMargins = this->lineEdit->contentsMargins();
                        break;
                    }

                    case QEvent::LayoutDirectionChange:
                    {
                        anchor->setEdges(anchorEdges());
                        updateIndicator();
                        break;
                    }

                    default:
                        break;
                }
            });

        NX_ASSERT(eventSignalizer);

        anchor->setEdges(anchorEdges());

        connect(lineEdit, &QLineEdit::textChanged, q,
            [this](const QString& text)
            {
                QnPasswordInformation newInformation(text);
                if (newInformation != currentInformation)
                {
                    currentInformation = newInformation;
                    updateIndicator();
                }
            });
    }

    void updateIndicator()
    {
        if (lineEdit.isNull())
            return;

        Q_Q(QnPasswordStrengthIndicator);
        QSize textSize = q->fontMetrics().size(Qt::TextSingleLine, currentInformation.text());
        q->resize(qMax(textSize.width() + textMargins, q->minimumWidth()), q->height());

        int extraContentMargin = q->width() + q->indicatorMargins().right() * 2;
        QMargins newMargins = lineEditContentsMargins;

        if (lineEdit->layoutDirection() == Qt::RightToLeft)
            newMargins.setLeft(qMax(newMargins.left(), extraContentMargin));
        else
            newMargins.setRight(qMax(newMargins.right(), extraContentMargin));

        QSignalBlocker blockEventSignals(eventSignalizer);
        lineEdit->setContentsMargins(newMargins);

        q->setToolTip(currentInformation.hint());

        q->update();
    }

    Qt::Edges anchorEdges() const
    {
        NX_ASSERT(lineEdit);
        Qt::LayoutDirection dir = lineEdit->layoutDirection();
        const int vertical = Qt::TopEdge | Qt::BottomEdge;
        const int horizontal = (dir == Qt::RightToLeft) ? Qt::LeftEdge : Qt::RightEdge;
        return Qt::Edges(horizontal | vertical);
    }

    QPointer<QLineEdit> lineEdit;
    QnPasswordInformation currentInformation;

    QnPasswordStrengthColors colors;
    qreal roundingRadius;
    int textMargins;

    QnWidgetAnchor* anchor;
    QMargins lineEditContentsMargins;
    QnAbstractEventSignalizer* eventSignalizer;

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
    return d->currentInformation;
}

const QnPasswordStrengthColors& QnPasswordStrengthIndicator::colors() const
{
    Q_D(const QnPasswordStrengthIndicator);
    return d->colors;
}

void QnPasswordStrengthIndicator::setColors(const QnPasswordStrengthColors& colors)
{
    Q_D(QnPasswordStrengthIndicator);
    if (d->colors == colors)
        return;

    d->colors = colors;
    update();
}

const QMargins& QnPasswordStrengthIndicator::indicatorMargins() const
{
    Q_D(const QnPasswordStrengthIndicator);
    return d->anchor->margins();
}

void QnPasswordStrengthIndicator::setIndicatorMargins(const QMargins& margins)
{
    Q_D(QnPasswordStrengthIndicator);
    d->anchor->setMargins(margins);
    d->updateIndicator();
}

qreal QnPasswordStrengthIndicator::roundingRadius() const
{
    Q_D(const QnPasswordStrengthIndicator);
    return d->roundingRadius;
}

void QnPasswordStrengthIndicator::setRoundingRadius(qreal radius)
{
    Q_D(QnPasswordStrengthIndicator);
    if (d->roundingRadius == radius)
        return;

    d->roundingRadius = radius;
    update();
}

int QnPasswordStrengthIndicator::textMargins() const
{
    Q_D(const QnPasswordStrengthIndicator);
    return d->textMargins;
}

void QnPasswordStrengthIndicator::setTextMargins(int margins)
{
    Q_D(QnPasswordStrengthIndicator);
    if (d->textMargins == margins)
        return;

    d->textMargins = margins;
    d->updateIndicator();
}

void QnPasswordStrengthIndicator::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event)
    Q_D(const QnPasswordStrengthIndicator);

    QColor background;
    switch (d->currentInformation.acceptance())
    {
        case QnPasswordInformation::Good:
            background = d->colors.good;
            break;

        case QnPasswordInformation::Acceptable:
            background = d->colors.acceptable;
            break;

        default:
            background = d->colors.inacceptable;
            break;
    }

    QPainter painter(this);
    painter.setPen(Qt::NoPen);
    painter.setBrush(background);

    painter.setRenderHint(QPainter::Antialiasing);
    painter.drawRoundedRect(rect().adjusted(0.5, 0.5, -0.5, -0.5), d->roundingRadius, d->roundingRadius);

    painter.setPen(palette().color(QPalette::Shadow));
    painter.drawText(rect(), Qt::TextSingleLine | Qt::AlignCenter, d->currentInformation.text());
}
