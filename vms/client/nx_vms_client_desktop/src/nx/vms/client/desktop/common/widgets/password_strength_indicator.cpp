// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "password_strength_indicator.h"

#include <QtCore/QPointer>
#include <QtGui/QPainter>
#include <QtWidgets/QLineEdit>

#include <nx/vms/client/desktop/style/style.h>

#include <nx/vms/client/desktop/common/widgets/line_edit_controls.h>
#include <nx/vms/client/desktop/ui/common/color_theme.h>

namespace
{
    /* Rounding radius of indicator rectangle: */
    static constexpr qreal kDefaultRoundingRadius = 2.5;

    /* Text margins inside indicator rectangle: */
    static const QMargins kDefaultTextMargins(4, 3, 4, 2);

    /* Minimum width of indicator rectangle: */
    static constexpr int kMinimumWidth = 40;
}

namespace nx::vms::client::desktop {

/*
 * QnPasswordStrengthIndicatorPrivate
 */

struct PasswordStrengthIndicator::Private
{
    Private(
        PasswordStrengthIndicator* q,
        QLineEdit* lineEdit,
        PasswordInformation::AnalyzeFunction analyzeFunction)
        :
        q(q),
        lineEdit(lineEdit),
        currentInformation(lineEdit->text(), analyzeFunction)
    {
        QObject::connect(lineEdit, &QLineEdit::textChanged, q,
            [this, analyzeFunction](const QString& text)
            {
                PasswordInformation newInformation(text, analyzeFunction);
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

    PasswordStrengthIndicator* const q = nullptr;

    const QPointer<QLineEdit> lineEdit;
    PasswordInformation currentInformation;

    qreal roundingRadius = kDefaultRoundingRadius;
    QMargins textMargins = kDefaultTextMargins;
    QSize minimumSizeHint;
};

/*
* PasswordStrengthIndicator
*/

PasswordStrengthIndicator::PasswordStrengthIndicator(
    QLineEdit* lineEdit,
    PasswordInformation::AnalyzeFunction analyzeFunction):
    base_type(),
    d(new Private(this, lineEdit, analyzeFunction))
{
    QFont textFont = font();
    textFont.setPixelSize(10);
    textFont.setBold(true);
    textFont.setCapitalization(QFont::AllUppercase);
    setFont(textFont);
    setCursor(Qt::ArrowCursor);

    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    d->updateIndicator();

    LineEditControls::get(lineEdit)->addControl(this, /* prepend */ true);
}

PasswordStrengthIndicator::~PasswordStrengthIndicator()
{
}

const PasswordInformation& PasswordStrengthIndicator::currentInformation() const
{
    return d->currentInformation;
}

qreal PasswordStrengthIndicator::roundingRadius() const
{
    return d->roundingRadius;
}

void PasswordStrengthIndicator::setRoundingRadius(qreal radius)
{
    if (d->roundingRadius == radius)
        return;

    d->roundingRadius = radius;
    update();
}

QMargins PasswordStrengthIndicator::textMargins() const
{
    return d->textMargins;
}

void PasswordStrengthIndicator::setTextMargins(const QMargins& value)
{
    if (d->textMargins == value)
        return;

    d->textMargins = value;
    d->updateIndicator();
}

void PasswordStrengthIndicator::paintEvent(QPaintEvent* /*event*/)
{
    QColor background;
    switch (d->currentInformation.acceptance())
    {
        case utils::PasswordAcceptance::Good:
            background = colorTheme()->color("green_core");
            break;

        case utils::PasswordAcceptance::Acceptable:
            background = colorTheme()->color("yellow_core");
            break;

        default:
            background = colorTheme()->color("red_core");
            break;
    }

    QPainter painter(this);
    painter.setPen(Qt::NoPen);
    painter.setBrush(background);

    const QRectF rect(this->rect());

    painter.setRenderHint(QPainter::Antialiasing);
    painter.drawRoundedRect(rect.adjusted(0.5, 0.5, -0.5, -0.5),
        d->roundingRadius, d->roundingRadius);

    painter.setPen(colorTheme()->color("dark5"));
    painter.drawText(rect.marginsRemoved(d->textMargins), Qt::TextSingleLine | Qt::AlignCenter,
        d->currentInformation.text());
}

QSize PasswordStrengthIndicator::minimumSizeHint() const
{
    return d->minimumSizeHint;
}

} // namespace nx::vms::client::desktop
