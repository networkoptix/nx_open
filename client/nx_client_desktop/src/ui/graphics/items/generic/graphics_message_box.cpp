#include "graphics_message_box.h"

#include <QtCore/QTimer>
#include <QtWidgets/QStyle>
#include <QtWidgets/QStyleOption>
#include <QtGui/QPainter>
#include <QtGui/QPainterPath>

#include <utils/common/scoped_painter_rollback.h>
#include <client/client_settings.h>

#include <ui/common/palette.h>
#include <ui/animation/opacity_animator.h>
#include <ui/graphics/items/standard/graphics_label.h>

namespace {

static const int kDefaultFontSize = 22;
static const int kDefaultRoundingRadius = 2;
static const int kDefaultTimeoutMs = 3000;
static const int kMessagesSpacing = 2;
static const int kHorizontalMargin = 24;
static const int kVerticalMargin = 8;

}

// -------------------------------------------------------------------------- //
// QnGraphicsMessageBoxHolder
// -------------------------------------------------------------------------- //
QnGraphicsMessageBoxHolder::QnGraphicsMessageBoxHolder(QGraphicsItem *parent):
    base_type(parent)
{
    m_layout = new QGraphicsLinearLayout(Qt::Vertical);
    m_layout->setContentsMargins(0.0, 0.0, 0.0, 0.0);
    m_layout->setSpacing(kMessagesSpacing);
    setLayout(m_layout);
    setAcceptedMouseButtons(Qt::NoButton);
}

QnGraphicsMessageBoxHolder::~QnGraphicsMessageBoxHolder()
{
}

void QnGraphicsMessageBoxHolder::addItem(QGraphicsLayoutItem *item)
{
    m_layout->addItem(item);
}

void QnGraphicsMessageBoxHolder::removeItem(QGraphicsLayoutItem *item)
{
    m_layout->removeItem(item);
}

QRectF QnGraphicsMessageBoxHolder::boundingRect() const
{
    return base_type::boundingRect();
}

void QnGraphicsMessageBoxHolder::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    base_type::paint(painter, option, widget);
}

// -------------------------------------------------------------------------- //
// QnGraphicsMessageBox
// -------------------------------------------------------------------------- //
QnGraphicsMessageBox::QnGraphicsMessageBox(QGraphicsItem *parent, const QString &text, int timeoutMsec):
    base_type(parent, Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool)
{
    m_label = new GraphicsLabel(this);
    m_label->setAlignment(Qt::AlignCenter);
    m_label->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    auto layout = new QGraphicsLinearLayout(Qt::Horizontal);
    layout->setContentsMargins(
        kHorizontalMargin,
        kVerticalMargin,
        kHorizontalMargin,
        kVerticalMargin);
    layout->addItem(m_label);
    setLayout(layout);

    m_timeout = timeoutMsec == 0 ? kDefaultTimeoutMs : timeoutMsec;

    setText(text);
    setFrameShape(Qn::RoundedRectangularFrame);
    setFrameWidth(0.0);
    setRoundingRadius(kDefaultRoundingRadius);

    QFont font = this->font();
    font.setPixelSize(kDefaultFontSize);
    font.setWeight(QFont::Light);
    setFont(font);
    setColors(QnGraphicsMessageBoxColors());
    setAcceptedMouseButtons(Qt::NoButton);
    setOpacity(0.0);

    showAnimated();
    QTimer::singleShot(m_timeout, this, &QnGraphicsMessageBox::hideAnimated);
}

QnGraphicsMessageBox::~QnGraphicsMessageBox()
{
}

QString QnGraphicsMessageBox::text() const
{
    return m_label->text();
}

void QnGraphicsMessageBox::setText(const QString &text)
{
    m_label->setText(text);
}

QnGraphicsMessageBoxColors QnGraphicsMessageBox::colors() const
{
    return m_colors;
}

void QnGraphicsMessageBox::setColors(const QnGraphicsMessageBoxColors &value)
{
    m_colors = value;

    setPaletteColor(this, QPalette::WindowText, m_colors.text);
    setFrameColor(m_colors.frame);
    setWindowColor(m_colors.window);
}

int QnGraphicsMessageBox::timeout() const
{
    return m_timeout;
}

QnGraphicsMessageBox* QnGraphicsMessageBox::information(const QString &text, int timeoutMsec)
{
    auto holder = QnGraphicsMessageBoxHolder::instance();
    NX_ASSERT(holder);
    if (!holder)
        return nullptr;

    QnGraphicsMessageBox* box = new QnGraphicsMessageBox(holder, text, timeoutMsec);
    holder->addItem(box);
    return box;
}

void QnGraphicsMessageBox::showAnimated()
{
    auto animator = opacityAnimator(this);
    animator->setEasingCurve(QEasingCurve::InQuad);
    animator->stop();
    if (qnSettings->lightMode().testFlag(Qn::LightModeNoAnimation))
        setOpacity(1.0);
    else
        animator->animateTo(1.0);

    //animator->setTimeLimit(200);
}

void QnGraphicsMessageBox::hideAnimated()
{
    if (qnSettings->lightMode().testFlag(Qn::LightModeNoAnimation))
    {
        hideImmideately();
        return;
    }

    auto animator = opacityAnimator(this);
    animator->stop();
    animator->setEasingCurve(QEasingCurve::OutQuad);
    animator->animateTo(0.0);
    connect(animator, &VariantAnimator::finished, this, &QnGraphicsMessageBox::hideImmideately);

    //animator->setTimeLimit(200);
}

void QnGraphicsMessageBox::hideImmideately()
{
    auto animator = opacityAnimator(this);
    disconnect(animator, nullptr, this, nullptr);
    if (animator->isRunning())
        animator->stop();
    setVisible(false);
    emit finished();
    deleteLater();
}

