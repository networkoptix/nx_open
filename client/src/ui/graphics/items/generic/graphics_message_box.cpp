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
    const int defaultFontSize = 22;
    const int defaultRoundingRadius = 18;
    const int defaultTimeout = 3000;
}

// -------------------------------------------------------------------------- //
// QnGraphicsMessageBoxItem
// -------------------------------------------------------------------------- //
QnGraphicsMessageBoxItem *instance = NULL;

QnGraphicsMessageBoxItem::QnGraphicsMessageBoxItem(QGraphicsItem *parent):
    base_type(parent)
{
    instance = this;
    m_layout = new QGraphicsLinearLayout(Qt::Vertical);
    m_layout->setContentsMargins(0.0, 0.0, 0.0, 0.0);
    m_layout->setSpacing(2.0);
    setLayout(m_layout);
    setAcceptedMouseButtons(Qt::NoButton);

    setOpacity(0.8);
}

QnGraphicsMessageBoxItem::~QnGraphicsMessageBoxItem() {
    // instance = NULL;
    // TODO: #GDM #Common why is it commented out?
}

void QnGraphicsMessageBoxItem::addItem(QGraphicsLayoutItem *item) {
    m_layout->addItem(item);
}

void QnGraphicsMessageBoxItem::removeItem(QGraphicsLayoutItem *item) {
    m_layout->removeItem(item);
}

QRectF QnGraphicsMessageBoxItem::boundingRect() const {
    return base_type::boundingRect();
}

void QnGraphicsMessageBoxItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) {
    base_type::paint(painter, option, widget);
}

// -------------------------------------------------------------------------- //
// QnGraphicsMessageBox
// -------------------------------------------------------------------------- //
QnGraphicsMessageBox::QnGraphicsMessageBox(QGraphicsItem *parent, const QString &text, int timeoutMsec, int fontSize):
    base_type(parent, Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool)
{
    m_label = new GraphicsLabel(this);
    m_label->setAlignment(Qt::AlignCenter);

    QGraphicsLinearLayout *layout = new QGraphicsLinearLayout(Qt::Horizontal);
    layout->addItem(m_label);
    setLayout(layout);

    m_timeout = timeoutMsec == 0 ? defaultTimeout : timeoutMsec;

    setText(text);
    setFrameShape(Qn::RoundedRectangularFrame);
    setRoundingRadius(defaultRoundingRadius);

    QFont font = this->font();
    font.setPixelSize(fontSize == 0 ? defaultFontSize : fontSize);
    setFont(font);

    setTextColor(QColor(166, 166, 166));
    setFrameColor(QColor(83, 83, 83));
    setWindowColor(QColor(33, 33, 80));

    setAcceptedMouseButtons(Qt::NoButton);

    if (qnSettings->lightMode() & Qn::LightModeNoAnimation) {
        QTimer::singleShot(m_timeout, this, SLOT(hideImmideately()));
    } else {
        setOpacity(0.0);
        VariantAnimator *animator = opacityAnimator(this);
        animator->setEasingCurve(QEasingCurve::Linear);
        animator->animateTo(1.0);
        connect(animator, &AbstractAnimator::finished, this, &QnGraphicsMessageBox::at_animationIn_finished);
    }
}

QnGraphicsMessageBox::~QnGraphicsMessageBox() {
}

QString QnGraphicsMessageBox::text() const {
    return m_label->text();
}

void QnGraphicsMessageBox::setText(const QString &text) {
    m_label->setText(text);
}

const QColor &QnGraphicsMessageBox::textColor() const {
    return palette().color(QPalette::WindowText);
}

void QnGraphicsMessageBox::setTextColor(const QColor &textColor) {
    setPaletteColor(this, QPalette::WindowText, textColor);
}

int QnGraphicsMessageBox::timeout() const {
    return m_timeout;
}

QnGraphicsMessageBox* QnGraphicsMessageBox::information(const QString &text, int timeoutMsec, int fontSize) {
    if (!instance)
        return NULL;

    QnGraphicsMessageBox* box = new QnGraphicsMessageBox(instance, text, timeoutMsec, fontSize);
    instance->addItem(box);
    return box;
}

QnGraphicsMessageBox* QnGraphicsMessageBox::informationTicking(const QString &text, int timeoutMsec, int fontSize) {
    if (!instance)
        return NULL;

    QPointer<QnGraphicsMessageBox> box = new QnGraphicsMessageBox(instance, QString(), timeoutMsec, fontSize);
    instance->addItem(box);

    const auto tickHandler = [box, text](int tick) {
        if (!box)
            return;

        int left = box->timeout() - tick;
        int n = (left + 500) / 1000;

        if (n > 0)
            box->setText(text.arg(n));
        else
            box->hideImmideately();
    };
    connect(box.data(), &QnGraphicsMessageBox::tick, instance, tickHandler);
    tickHandler(0);

    return box;
}

void QnGraphicsMessageBox::hideImmideately() {
    VariantAnimator *animator = opacityAnimator(this);
    disconnect(animator, 0, this, 0);

    if (animator->isRunning())
        animator->pause();
    animator->setDurationOverride(-1);
    animator->setSpeed(6.0);
    connect(animator, &VariantAnimator::finished, this, &QnGraphicsMessageBox::finished);
    connect(animator, &VariantAnimator::finished, this, &QObject::deleteLater);
    animator->animateTo(0.0);
}

void QnGraphicsMessageBox::at_animationIn_finished() {
    VariantAnimator *animator = opacityAnimator(this);
    animator->setTimeLimit(m_timeout);
    animator->setDurationOverride(m_timeout);
    animator->setEasingCurve(QEasingCurve::InCubic);
    animator->animateTo(0.6);
    disconnect(animator, 0, this, 0);
    connect(animator, &VariantAnimator::animationTick, this, &QnGraphicsMessageBox::tick);
    connect(animator, &VariantAnimator::finished, this, &QnGraphicsMessageBox::hideImmideately);
}

/*QSizeF QnGraphicsMessageBox::sizeHint(Qt::SizeHint which, const QSizeF &constraint) const {
    return base_type::sizeHint(which, constraint) + QSizeF(borderRadius * 2, borderRadius * 2);
}*/

