#include "notification_item.h"

#include <QtGui/QGraphicsLinearLayout>

#include <ui/animation/opacity_animator.h>
#include <ui/common/palette.h>
#include <ui/graphics/items/generic/image_button_widget.h>
#include <ui/graphics/items/generic/proxy_label.h>
#include <ui/graphics/items/generic/tool_tip_widget.h>
#include <ui/graphics/items/notifications/notification_item.h>
#include <ui/processors/hover_processor.h>

namespace {

    class QnNotificationToolTipItem: public QnToolTipWidget {
        typedef QnToolTipWidget base_type;
    public:
        QnNotificationToolTipItem(QGraphicsItem *parent = 0): base_type(parent)
        {
            setContentsMargins(5.0, 5.0, 5.0, 5.0);
            setTailWidth(5.0);

            setPaletteColor(this, QPalette::WindowText, QColor(63, 159, 216));
            setWindowBrush(QColor(0, 0, 0, 255));
            setFrameBrush(QColor(203, 210, 233, 128));
            setFrameWidth(1.0);

            QFont fixedFont = QApplication::font();
            fixedFont.setPixelSize(14);
            setFont(fixedFont);

            updateTailPos();
        }

    protected:
        virtual void resizeEvent(QGraphicsSceneResizeEvent *event) override {
            base_type::resizeEvent(event);

            updateTailPos();
        }

    private:
        void updateTailPos() {
            QRectF rect = this->rect();
            setTailPos(QPointF(rect.right() + 10.0, (rect.top() + rect.bottom()) / 2));
        }
    };

    const char *actionIndexPropertyName = "_qn_actionIndex";

    const qreal margin = 4;
    const qreal buttonHeight = 24;
} // anonymous namespace

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
    m_tooltipItem->setText(text);
}

QColor QnNotificationItem::color() const {
    return m_color;
}

void QnNotificationItem::setColor(const QColor &color) {
    m_color = color;
}

void QnNotificationItem::addActionButton(const QIcon &icon, const QString &tooltip, Qn::ActionId actionId, const QnActionParameters &parameters) {
    QnImageButtonWidget *button = new QnImageButtonWidget(this);
    button->setIcon(icon);
    button->setToolTip(tooltip);
    button->setMinimumSize(QSizeF(buttonHeight, buttonHeight));
    button->setMaximumSize(QSizeF(buttonHeight, buttonHeight));
    button->setProperty(actionIndexPropertyName, m_actions.size());
    m_layout->addItem(button);

    connect(button, SIGNAL(clicked()), this, SLOT(at_button_clicked()));
    m_actions << ActionData(actionId, parameters);
}

void QnNotificationItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) {
    Q_UNUSED(option)
    Q_UNUSED(widget)

    QRadialGradient gradient(margin, buttonHeight*.5 + margin, buttonHeight*2);
    gradient.setColorAt(0.0, m_color);
    QColor gradientTo(m_color);
    gradientTo.setAlpha(64);
    gradient.setColorAt(0.5, gradientTo);
    gradient.setColorAt(1.0, Qt::transparent);

    gradient.setSpread(QGradient::PadSpread);
    painter->fillRect(boundingRect(), QBrush(gradient));
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
