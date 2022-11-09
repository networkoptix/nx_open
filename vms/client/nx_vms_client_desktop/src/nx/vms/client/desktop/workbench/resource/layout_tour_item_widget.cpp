// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "layout_tour_item_widget.h"

#include <QtCore/QScopedValueRollback>
#include <QtGui/QGuiApplication>
#include <QtWidgets/QGraphicsLinearLayout>
#include <QtWidgets/QSpinBox>
#include <QtWidgets/QStyleOptionGraphicsItem>

#include <qt_graphics_items/graphics_label.h>
#include <qt_graphics_items/graphics_pixmap.h>

#include <core/resource/camera_resource.h>
#include <core/resource/media_resource.h>
#include <nx/utils/string.h>
#include <nx/vms/client/desktop/image_providers/camera_thumbnail_manager.h>
#include <nx/vms/client/desktop/layout/layout_data_helper.h>
#include <nx/vms/client/desktop/resource/layout_resource.h>
#include <nx/vms/client/desktop/style/resource_icon_cache.h>
#include <nx/vms/client/desktop/style/skin.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/ui/actions/action_manager.h>
#include <nx/vms/client/desktop/ui/common/color_theme.h>
#include <nx/vms/client/desktop/ui/graphics/painters/layout_preview_painter.h>
#include <nx/vms/text/time_strings.h>
#include <ui/common/palette.h>
#include <ui/graphics/items/generic/image_button_widget.h>
#include <ui/graphics/items/generic/masked_proxy_widget.h>
#include <ui/graphics/items/generic/viewport_bound_widget.h>
#include <ui/graphics/items/resource/button_ids.h>
#include <ui/workaround/widgets_signals_workaround.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_item.h>
#include <ui/workbench/workbench_layout.h>
#include <utils/common/event_processors.h>

namespace nx::vms::client::desktop {
namespace ui {
namespace workbench {

namespace {

class MouseEaterWidget: public QGraphicsWidget
{
public:
    MouseEaterWidget()
    {
        setAcceptedMouseButtons(Qt::LeftButton);
        auto eventEater = new QnMultiEventEater(Qn::AcceptEvent, this);
        eventEater->addEventType(QEvent::GraphicsSceneMousePress);
        eventEater->addEventType(QEvent::GraphicsSceneMouseRelease);
        eventEater->addEventType(QEvent::GraphicsSceneMouseDoubleClick);
        installEventFilter(eventEater);
    }
};

class LayoutPreviewWidget: public QGraphicsWidget
{
public:
    LayoutPreviewWidget(QSharedPointer<LayoutPreviewPainter> previewPainter):
        m_previewPainter(previewPainter)
    {
        setAcceptedMouseButtons(Qt::NoButton);
    }

    virtual void paint(QPainter* painter,
        const QStyleOptionGraphicsItem* option, QWidget* /*widget*/) override
    {
        if (auto previewPainter = m_previewPainter.lock())
            previewPainter->paint(painter, option->rect);
    }

private:
    QWeakPointer<LayoutPreviewPainter> m_previewPainter;
};

static const int kMargin = 8; //< Pixels.
static const int kSpacing = 8; //< Pixels.

static constexpr int kMaxTitleLength = 30; //< symbols

} // namespace

LayoutTourItemWidget::LayoutTourItemWidget(
    SystemContext* systemContext,
    WindowContext* windowContext,
    QnWorkbenchItem* item,
    QGraphicsItem* parent)
    :
    base_type(systemContext, windowContext, item, parent),
    m_previewPainter(new LayoutPreviewPainter())
{
    setPaletteColor(this, QPalette::Highlight, colorTheme()->color("brand_core"));
    setPaletteColor(this, QPalette::Dark, colorTheme()->color("dark17"));
    setOption(QnResourceWidget::InfoOverlaysForbidden);
    setOption(QnResourceWidget::WindowRotationForbidden);

    QnLayoutResourcePtr layout = resource().dynamicCast<QnLayoutResource>();

    if (!layout)
    {
        layout = layoutFromResource(resource());
        QString name = lit("Tour@%1").arg(resource()->getName());
        layout->setName(name);

        connect(resource().get(), &QnResource::rotationChanged, this,
            [this]()
            {
                m_previewPainter->setLayout(layoutFromResource(resource()));
            });
    }

    m_previewPainter->setLayout(layout);

    initOverlay();
}

LayoutTourItemWidget::~LayoutTourItemWidget()
{
}

Qn::ResourceStatusOverlay LayoutTourItemWidget::calculateStatusOverlay() const
{
    return Qn::EmptyOverlay;
}

int LayoutTourItemWidget::calculateButtonsVisibility() const
{
    return Qn::CloseButton;
}

void LayoutTourItemWidget::initOverlay()
{
    auto font = this->font();
    font.setPixelSize(14);
    setFont(font);

    auto titleFont(font);
    titleFont.setWeight(QFont::DemiBold);

    auto orderFont(font);
    orderFont.setPixelSize(18);

    auto icon = new GraphicsPixmap();
    icon->setAcceptedMouseButtons(Qt::NoButton);

    auto updateIcon = [this, icon]
        {
            auto pixmap = qnResIconCache->icon(resource())
                .pixmap(1024, 1024, QIcon::Normal, QIcon::On);
            pixmap.setDevicePixelRatio(qApp->devicePixelRatio());
            icon->setPixmap(pixmap);
        };
    updateIcon();
    connect(resource().get(), &QnResource::parentIdChanged, this, updateIcon);
    connect(resource().get(), &QnResource::statusChanged, this, updateIcon);

    auto title = new GraphicsLabel();
    title->setAcceptedMouseButtons(Qt::NoButton);
    title->setPerformanceHint(GraphicsLabel::PixmapCaching);
    title->setFont(titleFont);
    title->setAlignment(Qt::AlignVCenter);

    auto updateTitle = [this, title]
        {
            // TODO: #sivanov Elide on the widget level.
            title->setText(nx::utils::elideString(resource()->getName(), kMaxTitleLength));
        };
    updateTitle();
    connect(resource().get(), &QnResource::nameChanged, this, updateTitle);

    auto closeButton = new QnImageButtonWidget();
    const auto closeButtonIcon = qnSkin->icon(lit("text_buttons/clear.png"));
    const auto closeButtonSize = Skin::maximumSize(closeButtonIcon);
    closeButton->setIcon(closeButtonIcon);
    closeButton->setFixedSize(closeButtonSize);
    connect(closeButton, &QnImageButtonWidget::clicked, this, &QnResourceWidget::close,
        Qt::QueuedConnection);

    auto headerLayout = new QGraphicsLinearLayout(Qt::Horizontal);
    headerLayout->addItem(icon);
    headerLayout->addItem(title);
    headerLayout->addStretch();
    headerLayout->addItem(closeButton);
    headerLayout->setSpacing(4.0);

    auto orderLabel = new GraphicsLabel();
    orderLabel->setPerformanceHint(GraphicsLabel::PixmapCaching);
    orderLabel->setAcceptedMouseButtons(Qt::NoButton);
    orderLabel->setAlignment(Qt::AlignVCenter);
    orderLabel->setFont(orderFont);
    auto updateOrder =
        [this, orderLabel](Qn::ItemDataRole role)
        {
            if (role != Qn::LayoutTourItemOrderRole)
                return;

            const int order = item()->data(role).toInt();
            orderLabel->setText(QString::number(order));
        };
    updateOrder(Qn::LayoutTourItemOrderRole);
    connect(item(), &QnWorkbenchItem::dataChanged, this, updateOrder);

    auto updateLightText =
        [this, title, orderLabel]
        {
            setPaletteColor(title, QPalette::WindowText, palette().color(QPalette::Light));
            setPaletteColor(orderLabel, QPalette::WindowText, palette().color(QPalette::Light));
        };
    installEventHandler(this, QEvent::PaletteChange, this, updateLightText);
    updateLightText();

    auto delayHintLabel = new GraphicsLabel();
    delayHintLabel->setPerformanceHint(GraphicsLabel::PixmapCaching);
    delayHintLabel->setAcceptedMouseButtons(Qt::NoButton);
    delayHintLabel->setAlignment(Qt::AlignVCenter);
    delayHintLabel->setFont(font);

    auto updateHint =
        [this, delayHintLabel]
        {
            const bool isManual = item()->layout()
                && item()->layout()->data(Qn::LayoutTourIsManualRole).toBool();
            QColor textColor = palette().color(QPalette::Dark);
            QString text = isManual
                ? tr("Switch by", "Arrows will follow")
                    + QString::fromWCharArray(L" \x2190 \x2192") //< Arrows.
                : tr("Display for", "Time selector will follow");

            if (!isManual)
            {
                switch (selectionState())
                {
                    case SelectionState::selected:
                    case SelectionState::focusedAndSelected:
                        textColor = palette().color(QPalette::Highlight);
                        text = tr("Display selected for", "Time will follow");
                        break;
                    default:
                        break;
                }
            }
            setPaletteColor(delayHintLabel, QPalette::WindowText, textColor);
            delayHintLabel->setText(text);
        };

    connect(this, &QnResourceWidget::selectionStateChanged, this, updateHint);
    installEventHandler(this, QEvent::PaletteChange, this, updateHint);
    updateHint();

    auto delayEdit = new QSpinBox();
    delayEdit->setSuffix(' ' + QnTimeStrings::suffix(QnTimeStrings::Suffix::Seconds));
    delayEdit->setMinimum(1);
    delayEdit->setMaximum(99);
    const auto delayMs = item()->data(Qn::LayoutTourItemDelayMsRole).toInt();
    delayEdit->setValue(delayMs / 1000);

    connect(delayEdit, QnSpinboxIntValueChanged, this,
        [this](int value)
        {
            if (m_updating)
                return;

            QScopedValueRollback<bool> guard(m_updating, true);
            item()->setData(Qn::LayoutTourItemDelayMsRole, value * 1000);

            // Store visual data to the tour and then add it to the save queue.
            menu()->trigger(action::SaveCurrentLayoutTourAction);
        });

    connect(layoutResource().get(),
        &LayoutResource::itemDataChanged,
        this,
        [this, delayEdit](const QnUuid& id, Qn::ItemDataRole role, const QVariant& data)
        {
            if (m_updating || role != Qn::LayoutTourItemDelayMsRole || id != item()->uuid())
                return;

            QScopedValueRollback<bool> guard(m_updating, true);
            delayEdit->setValue(data.toInt() / 1000);
        });

    auto delayWidget = new QnMaskedProxyWidget();
    delayWidget->setWidget(delayEdit);
    delayWidget->setFocusPolicy(Qt::ClickFocus);

    auto footerLayout = new QGraphicsLinearLayout(Qt::Horizontal);
    footerLayout->addItem(orderLabel);
    footerLayout->addStretch();
    footerLayout->addItem(delayHintLabel);
    footerLayout->addItem(delayWidget);
    // Additional top margin to compensate outer preview border.
    footerLayout->setContentsMargins(0, 2, 0, 0);
    footerLayout->setSpacing(kSpacing);

    auto footerWidget = new MouseEaterWidget();
    footerWidget->setLayout(footerLayout);

    auto updateManualMode = [this, delayWidget]
        {
            const bool isManual = item()->layout()->data(Qn::LayoutTourIsManualRole).toBool();
            delayWidget->setVisible(!isManual);
        };

    connect(item()->layout(), &QnWorkbenchLayout::dataChanged, this,
        [updateManualMode, updateHint](int role)
        {
            if (role == Qn::LayoutTourIsManualRole)
            {
                updateManualMode();
                updateHint();
            }
        });
    updateManualMode();

    auto contentWidget = new LayoutPreviewWidget(m_previewPainter);

    auto layout = new QGraphicsLinearLayout(Qt::Vertical);
    layout->setContentsMargins(kMargin, kMargin, kMargin, kMargin);
    layout->addItem(headerLayout);
    layout->addItem(contentWidget);
    layout->addItem(footerWidget);
    layout->setSpacing(kSpacing);
    layout->setStretchFactor(contentWidget, 1000);

    auto overlayWidget = new QnViewportBoundWidget(this);
    overlayWidget->setLayout(layout);
    overlayWidget->setAcceptedMouseButtons(Qt::NoButton);
    overlayWidget->setAutoFillBackground(true);
    setPaletteColor(overlayWidget, QPalette::Window, qApp->palette().color(QPalette::Dark));
    addOverlayWidget(overlayWidget, Visible);
}

} // namespace workbench
} // namespace ui
} // namespace nx::vms::client::desktop
