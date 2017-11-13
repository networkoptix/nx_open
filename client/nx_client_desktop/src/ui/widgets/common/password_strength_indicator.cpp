#include "password_strength_indicator.h"

#include <QtGui/QPainter>
#include <QtWidgets/QLineEdit>

#include <ui/style/nx_style.h>
#include <utils/common/connective.h>

#include <nx/client/desktop/ui/common/line_edit_controls.h>

using namespace nx::client::desktop::ui;

namespace
{
    /* Rounding radius of indicator rectangle: */
    static constexpr qreal kDefaultRoundingRadius = 2.5;

    /* Text margins inside indicator rectangle: */
    static const QMargins kDefaultTextMargins(4, 3, 4, 2);

    /* Minimum width of indicator rectangle: */
    static constexpr int kMinimumWidth = 40;
}

/*
 * QnPasswordStrengthIndicatorPrivate
 */

class QnPasswordStrengthIndicatorPrivate: public ConnectiveBase
{
    QnPasswordStrengthIndicatorPrivate(
        QnPasswordStrengthIndicator* q,
        QLineEdit* lineEdit,
        QnPasswordInformation::AnalyzeFunction analyzeFunction) :
        lineEdit(lineEdit),
        currentInformation(lineEdit->text(), analyzeFunction),
        q_ptr(q)
    {
        connect(lineEdit, &QLineEdit::textChanged, q,
            [this, analyzeFunction](const QString& text)
            {
                QnPasswordInformation newInformation(text, analyzeFunction);
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
        auto size = q->fontMetrics().size(Qt::TextSingleLine, currentInformation.text());
        size.setWidth(qMax(kMinimumWidth, size.width() + textMargins.left() + textMargins.right()));
        size.setHeight(size.height() + textMargins.top() + textMargins.bottom());

        if (minimumSizeHint != size)
        {
            minimumSizeHint = size;
            q->updateGeometry();
        }

        q->setToolTip(currentInformation.hint());
        q->update();
    }

    QPointer<QLineEdit> lineEdit;
    QnPasswordInformation currentInformation;

    QnPasswordStrengthColors colors;
    qreal roundingRadius = kDefaultRoundingRadius;
    QMargins textMargins = kDefaultTextMargins;
    QSize minimumSizeHint;

    QnPasswordStrengthIndicator* q_ptr;
    Q_DECLARE_PUBLIC(QnPasswordStrengthIndicator)
};

/*
* QnPasswordStrengthIndicator
*/

QnPasswordStrengthIndicator::QnPasswordStrengthIndicator(
    QLineEdit* lineEdit,
    QnPasswordInformation::AnalyzeFunction analyzeFunction):
    base_type(),
    d_ptr(new QnPasswordStrengthIndicatorPrivate(this, lineEdit, analyzeFunction))
{
    QFont textFont = font();
    textFont.setPixelSize(10);
    textFont.setBold(true);
    textFont.setCapitalization(QFont::AllUppercase);
    setFont(textFont);
    setCursor(Qt::ArrowCursor);

    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    d_ptr->updateIndicator();

    LineEditControls::get(lineEdit)->addControl(this);
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

QMargins QnPasswordStrengthIndicator::textMargins() const
{
    Q_D(const QnPasswordStrengthIndicator);
    return d->textMargins;
}

void QnPasswordStrengthIndicator::setTextMargins(const QMargins& value)
{
    Q_D(QnPasswordStrengthIndicator);
    if (d->textMargins == value)
        return;

    d->textMargins = value;
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

    const auto rect = this->rect();

    painter.setRenderHint(QPainter::Antialiasing);
    painter.drawRoundedRect(rect.adjusted(0.5, 0.5, -0.5, -0.5),
        d->roundingRadius, d->roundingRadius);

    painter.setPen(d->colors.text);
    painter.drawText(rect.marginsRemoved(d->textMargins), Qt::TextSingleLine | Qt::AlignCenter,
        d->currentInformation.text());
}

QSize QnPasswordStrengthIndicator::minimumSizeHint() const
{
    Q_D(const QnPasswordStrengthIndicator);
    return d->minimumSizeHint;
}
