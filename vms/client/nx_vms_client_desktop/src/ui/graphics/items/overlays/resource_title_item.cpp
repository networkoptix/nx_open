// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "resource_title_item.h"

#include <QtWidgets/QGraphicsLinearLayout>

#include <ui/animation/opacity_animator.h>
#include <nx/vms/client/desktop/common/utils/painter_transform_scale_stripper.h>
#include <nx/vms/client/desktop/ui/common/color_theme.h>
#include <ui/common/palette.h>
#include <qt_graphics_items/graphics_label.h>
#include <ui/graphics/items/generic/image_button_bar.h>

using namespace nx::vms::client::desktop;

namespace {

GraphicsLabel* createGraphicsLabel()
{
    auto label = new GraphicsLabel();
    label->setAcceptedMouseButtons(Qt::NoButton);
    label->setPerformanceHint(GraphicsLabel::PixmapCaching);
    label->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    label->setElideMode(Qt::ElideRight);
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

    m_leftButtonsPanel->setObjectName("LeftButtonPanel");
    m_rightButtonsPanel->setObjectName("RightButtonPanel");
    m_leftButtonsPanel->setSpacing(1);
    m_rightButtonsPanel->setSpacing(1);

    static constexpr int kLayoutSpacing = 2;
    auto mainLayout = new QGraphicsLinearLayout(Qt::Horizontal);
    mainLayout->setContentsMargins(4.0, 4.0, 4.0, 10.0);
    mainLayout->setSpacing(kLayoutSpacing);
    mainLayout->addItem(m_leftButtonsPanel);
    mainLayout->addItem(m_nameLabel);
    mainLayout->addItem(m_extraInfoLabel);
    mainLayout->addStretch();
    mainLayout->addItem(m_rightButtonsPanel);

    static constexpr qreal kShadowRadius = 2.5;

    m_nameLabel->setShadowRadius(kShadowRadius);
    m_extraInfoLabel->setShadowRadius(kShadowRadius);

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
    const auto paintRect = scaleStripper.mapRect(rect());

    QLinearGradient gradient(0, 0, 0, paintRect.height());
    gradient.setColorAt(0, colorTheme()->color("camera.titleGradient.top"));
    gradient.setColorAt(1, colorTheme()->color("camera.titleGradient.bottom"));
    QBrush brush(gradient);
    brush.setTransform(QTransform::fromTranslate(0, paintRect.top()));
    painter->fillRect(paintRect, brush);
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
