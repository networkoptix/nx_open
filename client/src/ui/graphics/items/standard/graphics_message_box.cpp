#include "graphics_message_box.h"

#include <QtWidgets/QStyle>
#include <QtWidgets/QStyleOption>
#include <QtGui/QPainter>
#include <QtGui/QPainterPath>

#include <ui/animation/opacity_animator.h>
#include <ui/graphics/items/standard/graphics_frame.h>

#include <utils/common/scoped_painter_rollback.h>

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
  //  instance = NULL;
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

//-----------------------QnGraphicsMessageBox---------------------------------------//

namespace {
    const int fontSize(22);
    const int borderRadius(18);
    const int defaultTimeout(3000);
    const QColor fontColor(166, 166, 166);
    const QColor borderColor(83, 83, 83);
    const QColor backgroundColor(33, 33, 80);
}


QnGraphicsMessageBox::QnGraphicsMessageBox(QGraphicsItem *parent, const QString &text, int timeoutMsec):
    base_type(parent, Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool)
{
    m_timeout = timeoutMsec == 0 ? defaultTimeout : timeoutMsec;

    setText(text);
    setFrameShape(GraphicsFrame::NoFrame);

    QFont font;
    font.setPixelSize(fontSize);
    setFont(font);

    QPalette palette = this->palette();
    palette.setColor(QPalette::WindowText, fontColor);
    setPalette(palette);

    setAcceptedMouseButtons(Qt::NoButton);
    setOpacity(0.0);

    VariantAnimator *animator = opacityAnimator(this);
    animator->setEasingCurve(QEasingCurve::Linear);
    animator->animateTo(1.0);
    connect(animator, SIGNAL(finished()), this, SLOT(at_animationIn_finished()));
}

QnGraphicsMessageBox::~QnGraphicsMessageBox() {
}

QnGraphicsMessageBox* QnGraphicsMessageBox::information(const QString &text) {

    if (!instance)
        return NULL;

    QnGraphicsMessageBox* box = new QnGraphicsMessageBox(instance, text);
    instance->addItem(box);
    return box;
}

int QnGraphicsMessageBox::timeout() const {
    return m_timeout;
}

void QnGraphicsMessageBox::hideImmideately() {
    VariantAnimator *animator = opacityAnimator(this);
    disconnect(animator, 0, this, 0);

    if (animator->isRunning())
        animator->pause();
    animator->setDurationOverride(-1);
    animator->setSpeed(6.0);
    connect(animator, SIGNAL(finished()), this, SIGNAL(finished()));
    connect(animator, SIGNAL(finished()), this, SLOT(deleteLater()));
    animator->animateTo(0.0);
}

void QnGraphicsMessageBox::at_animationIn_finished() {
    VariantAnimator *animator = opacityAnimator(this);
    animator->setTimeLimit(m_timeout);
    animator->setDurationOverride(m_timeout);
    animator->setEasingCurve(QEasingCurve::InCubic);
    animator->animateTo(0.6);
    disconnect(animator, 0, this, 0);
    connect(animator, SIGNAL(animationTick(int)), this, SIGNAL(tick(int)));
    connect(animator, SIGNAL(finished()), this, SIGNAL(finished()));
    connect(animator, SIGNAL(finished()), this, SLOT(deleteLater()));
}

QSizeF QnGraphicsMessageBox::sizeHint(Qt::SizeHint which, const QSizeF &constraint) const {
    return base_type::sizeHint(which, constraint) + QSizeF(borderRadius*2, borderRadius*2);
}

void QnGraphicsMessageBox::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) {
    QPainterPath path;
    path.addRoundedRect(rect(), borderRadius, borderRadius);

    QnScopedPainterPenRollback penRollback(painter, borderColor);
    QnScopedPainterBrushRollback brushRollback(painter, backgroundColor);
    painter->drawPath(path);
    Q_UNUSED(penRollback)
    Q_UNUSED(brushRollback)

    base_type::paint(painter, option, widget);
}

