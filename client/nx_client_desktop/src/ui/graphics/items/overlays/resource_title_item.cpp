#include "resource_title_item.h"

#include <QtWidgets/QGraphicsLinearLayout>

#include <ui/animation/opacity_animator.h>
#include <nx/client/desktop/ui/common/painter_transform_scale_stripper.h>
#include <ui/common/palette.h>
#include <ui/graphics/items/standard/graphics_label.h>
#include <ui/graphics/items/generic/image_button_bar.h>

using namespace nx::client::desktop::ui;

namespace {

GraphicsLabel* createGraphicsLabel()
{
    auto label = new GraphicsLabel();
    label->setAcceptedMouseButtons(Qt::NoButton);
    label->setPerformanceHint(GraphicsLabel::PixmapCaching);
    label->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    QFont font(label->font());
    font.setBold(true);
    label->setFont(font);
    return label;
}

} // namespace

QnResourceTitleItem::QnResourceTitleItem(QGraphicsItem* parent):
    base_type(parent),
    m_leftButtonsPanel(new QnImageButtonBar(this)),
    m_rightButtonsPanel(new QnImageButtonBar(this)),
    m_nameLabel(createGraphicsLabel()),
    m_extraInfoLabel(createGraphicsLabel())
{
    setAcceptedMouseButtons(Qt::NoButton);

    static const QColor overlayBackgroundColor = QColor(0, 0, 0, 96); // TODO: #gdm #vkutin #customization
    setPaletteColor(this, QPalette::Window, overlayBackgroundColor);

    static const QSizeF kButtonsSize(24, 24);
    m_leftButtonsPanel->setUniformButtonSize(kButtonsSize);
    m_rightButtonsPanel->setUniformButtonSize(kButtonsSize);

    static constexpr int kLayoutSpacing = 2;
    auto mainLayout = new QGraphicsLinearLayout(Qt::Horizontal);
    mainLayout->setContentsMargins(0.0, 0.0, 0.0, 0.0);
    mainLayout->setSpacing(kLayoutSpacing);
    mainLayout->addItem(leftButtonsBar());
    mainLayout->addItem(titleLabel());
    mainLayout->addItem(extraInfoLabel());
    mainLayout->addStretch();
    mainLayout->addItem(rightButtonsBar());

    setLayout(mainLayout);
}

QnResourceTitleItem::~QnResourceTitleItem()
{
}

void QnResourceTitleItem::paint(QPainter* painter,
    const QStyleOptionGraphicsItem* /*option*/,
    QWidget* /*widget*/)
{
    const PainterTransformScaleStripper scaleStripper(painter);
    painter->fillRect(scaleStripper.mapRect(rect()), palette().color(QPalette::Window));
}

void QnResourceTitleItem::setSimpleMode(bool isSimpleMode, bool animate)
{
    const auto targetValue = isSimpleMode ? 0.0 : 1.0;
    if (animate)
    {
        opacityAnimator(m_extraInfoLabel)->animateTo(targetValue);
        opacityAnimator(m_rightButtonsPanel)->animateTo(targetValue);
    }
    else
    {
        m_extraInfoLabel->setOpacity(targetValue);
        m_rightButtonsPanel->setOpacity(targetValue);
    }
}

QnImageButtonBar* QnResourceTitleItem::leftButtonsBar()
{
    return m_leftButtonsPanel;
}

QnImageButtonBar* QnResourceTitleItem::rightButtonsBar()
{
    return m_rightButtonsPanel;
}

GraphicsLabel* QnResourceTitleItem::titleLabel()
{
    return m_nameLabel;
}

GraphicsLabel* QnResourceTitleItem::extraInfoLabel()
{
    return m_extraInfoLabel;
}


