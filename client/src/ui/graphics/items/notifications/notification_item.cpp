
#include "notification_item.h"

#include <QtGui/QApplication>
#include <QtGui/QGraphicsLinearLayout>

#include <utils/math/color_transformations.h>

#include <ui/animation/opacity_animator.h>
#include <ui/common/palette.h>
#include <ui/graphics/items/generic/proxy_label.h>
#include <ui/graphics/items/notifications/notification_item.h>
#include <ui/processors/hover_processor.h>

namespace {
    const char *actionIndexPropertyName = "_qn_actionIndex";

    const qreal margin = 4;
    const qreal buttonSize = 24;
} // anonymous namespace

/********** QnNotificationToolTipItem *********************/

QnNotificationToolTipItem::QnNotificationToolTipItem(QGraphicsItem *parent):
    base_type(parent),
    m_textLabel(new QnProxyLabel(this)),
    m_thumbnailLabel(NULL)
{
    // TODO: Somehow unify it with the one in tool_tip_slider.cpp?
    // At least the styling part?
    setContentsMargins(5.0, 5.0, 5.0, 5.0);
    setTailWidth(5.0);

    setPaletteColor(this, QPalette::WindowText, QColor(63, 159, 216));
    setWindowBrush(QColor(0, 0, 0, 255));
    setFrameBrush(QColor(203, 210, 233, 128));
    setFrameWidth(1.0);

    QFont fixedFont = QApplication::font();
    fixedFont.setPixelSize(14);
    setFont(fixedFont);

    m_textLabel->setAlignment(Qt::AlignCenter);
    setPaletteColor(m_textLabel, QPalette::Window, Qt::transparent);

    QGraphicsLinearLayout *layout = new QGraphicsLinearLayout(Qt::Vertical);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addItem(m_textLabel);
    setLayout(layout);

    updateTailPos();
}

void QnNotificationToolTipItem::setThumbnail(const QImage &image) {
    if (!m_thumbnailLabel) {
        m_thumbnailLabel = new QnProxyLabel(this);
        m_thumbnailLabel->setAlignment(Qt::AlignCenter);
        setPaletteColor(m_thumbnailLabel, QPalette::Window, Qt::transparent);
        QGraphicsLinearLayout *layout = dynamic_cast<QGraphicsLinearLayout*>(this->layout());
        if (!layout)
            return;
        layout->insertItem(0, m_thumbnailLabel);
    }

    m_thumbnailLabel->setPixmap(QPixmap::fromImage(image));
}

void QnNotificationToolTipItem::setText(const QString &text) {
    m_textLabel->setText(text);
}

void QnNotificationToolTipItem::resizeEvent(QGraphicsSceneResizeEvent *event)  {
    base_type::resizeEvent(event);
    updateTailPos();
}

void QnNotificationToolTipItem::updateTailPos()  {
    QRectF rect = this->rect();
    setTailPos(QPointF(rect.right() + 10.0, (rect.top() + rect.bottom()) / 2));
}

/********** QnNotificationItem *********************/

QnNotificationItem::QnNotificationItem(QGraphicsItem *parent, Qt::WindowFlags flags) :
    base_type(parent, flags),
    m_layout(new QGraphicsLinearLayout(Qt::Horizontal)),
    m_textLabel(new QnProxyLabel(this)),
    m_color(Qt::transparent),
    m_hoverProcessor(new HoverFocusProcessor(this))
{
    m_tooltipItem = new QnNotificationToolTipItem(this);

    m_textLabel->setWordWrap(true);
    m_textLabel->setAlignment(Qt::AlignCenter);
    setPaletteColor(m_textLabel, QPalette::Window, Qt::transparent);

    m_layout->setContentsMargins(margin*2, margin, margin, margin);
    m_layout->addItem(m_textLabel);
    m_layout->setStretchFactor(m_textLabel, 1.0);

    setLayout(m_layout);

    setFrameColor(QColor(110, 110, 110, 255)); // TODO: Same as in workbench_ui. Unify?
    setFrameWidth(0.5);
    setWindowBrush(Qt::transparent);

    m_hoverProcessor->addTargetItem(this);
    m_hoverProcessor->addTargetItem(m_tooltipItem);
    m_hoverProcessor->setHoverEnterDelay(250);
    m_hoverProcessor->setHoverLeaveDelay(250);
    connect(m_hoverProcessor,    SIGNAL(hoverEntered()),  this,  SLOT(updateToolTipVisibility()));
    connect(m_hoverProcessor,    SIGNAL(hoverLeft()),     this,  SLOT(updateToolTipVisibility()));

    m_tooltipItem->setFocusProxy(this);
    m_tooltipItem->setOpacity(0.0);
    m_tooltipItem->setAcceptHoverEvents(true);
    m_tooltipItem->installEventFilter(this);
    m_tooltipItem->setFlag(ItemIgnoresParentOpacity, true);
    connect(m_tooltipItem, SIGNAL(tailPosChanged()), this, SLOT(updateToolTipPosition()));
    connect(this, SIGNAL(geometryChanged()), this, SLOT(updateToolTipPosition()));
    connect(this, SIGNAL(imageChanged(QImage)), m_tooltipItem, SLOT(setThumbnail(QImage)));

    updateToolTipPosition();
    updateToolTipVisibility();
}

QnNotificationItem::~QnNotificationItem() {
}

QString QnNotificationItem::text() const {
    return m_textLabel->text();
}

void QnNotificationItem::setText(const QString &text) {
    m_textLabel->setText(text);
}

void QnNotificationItem::setTooltipText(const QString &text) {
    m_tooltipItem->setText(text);
}

QColor QnNotificationItem::color() const {
    return m_color;
}

void QnNotificationItem::setColor(const QColor &color) {
    m_color = color;
}

void QnNotificationItem::setImage(const QImage &image) {
    m_image = image;
    emit imageChanged(image);
}

void QnNotificationItem::addActionButton(const QIcon &icon, const QString &tooltip, Qn::ActionId actionId,
                                         const QnActionParameters &parameters,
                                         const qreal sizeMultiplier, const bool isThumbnail) {
    QnImageButtonWidget *button;

    if (isThumbnail) {
        button = new QnThumbnailImageButtonWidget(this);
        connect(this, SIGNAL(imageChanged(QImage)), button, SLOT(setThumbnail(QImage)));
    }
    else
        button = new QnImageButtonWidget(this);
    button->setIcon(icon);
    button->setToolTip(tooltip);

    button->setFixedSize(buttonSize * sizeMultiplier);
    button->setProperty(actionIndexPropertyName, m_actions.size());
    m_layout->addItem(button);

    connect(button, SIGNAL(clicked()), this, SLOT(at_button_clicked()));
    m_actions << ActionData(actionId, parameters);
}

void QnNotificationItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) {
    base_type::paint(painter, option, widget);

    painter->setPen(QPen(QColor(110, 110, 110, 255), 0.5));
    painter->setBrush(QBrush(toTransparent(m_color, 0.5)));

    QRectF rect = this->rect();

    qreal side = qMin(qMin(rect.width(), rect.height()), 16.0) / 2.0;

    qreal left = rect.left();
    qreal xcenter = left + side * 0.5;
    
    qreal ycenter = (rect.top() + rect.bottom()) / 2;
    qreal top = ycenter - side;
    qreal bottom = ycenter + side;
    
    // TODO: cache the path?
    QPainterPath path;
    path.moveTo(left, top);
    path.lineTo(xcenter, top);
    path.arcTo(xcenter - side, ycenter - side, side * 2, side * 2, 90, -180);
    path.lineTo(left, bottom);
    path.closeSubpath();

    painter->drawPath(path);
}

void QnNotificationItem::hideToolTip() {
    opacityAnimator(m_tooltipItem, 2.0)->animateTo(0.0);
}

void QnNotificationItem::showToolTip() {
    opacityAnimator(m_tooltipItem, 2.0)->animateTo(1.0);
}

void QnNotificationItem::updateToolTipVisibility() {
    if(m_hoverProcessor->isHovered()) {
        showToolTip();
    } else {
        hideToolTip();
    }
}

void QnNotificationItem::updateToolTipPosition() {
    m_tooltipItem->pointTo(QPointF(0, geometry().height() / 2 ));
}

void QnNotificationItem::at_button_clicked() {
    int idx = sender()->property(actionIndexPropertyName).toInt();
    if (m_actions.size() <= idx)
        return;
    ActionData data = m_actions[idx];

    emit actionTriggered(data.action, data.params);
}
