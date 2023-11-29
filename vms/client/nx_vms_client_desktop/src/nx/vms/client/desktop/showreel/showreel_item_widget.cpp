// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "showreel_item_widget.h"

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
#include <nx/utils/unicode_chars.h>
#include <nx/vms/client/core/skin/color_theme.h>
#include <nx/vms/client/core/skin/skin.h>
#include <nx/vms/client/desktop/image_providers/camera_thumbnail_manager.h>
#include <nx/vms/client/desktop/layout/layout_data_helper.h>
#include <nx/vms/client/desktop/menu/action_manager.h>
#include <nx/vms/client/desktop/resource/layout_resource.h>
#include <nx/vms/client/desktop/style/resource_icon_cache.h>
#include <nx/vms/client/desktop/system_context.h>
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

namespace {

static const QColor klight10Color = "#A5B7C0";
static const nx::vms::client::core::SvgIconColorer::IconSubstitutions kIconSubstitutions = {
    {QIcon::Normal, {{klight10Color, "light10"}}},
    {QIcon::Active, {{klight10Color, "light11"}}},
    {QIcon::Selected, {{klight10Color, "light9"}}},
    {QnIcon::Error, {{klight10Color, "red_l2"}}},
};

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
    LayoutPreviewWidget(QSharedPointer<ui::LayoutPreviewPainter> previewPainter):
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
    QWeakPointer<ui::LayoutPreviewPainter> m_previewPainter;
};

static const int kMargin = 8; //< Pixels.
static const int kSpacing = 8; //< Pixels.

static constexpr int kMaxTitleLength = 30; //< symbols

} // namespace

ShowreelItemWidget::ShowreelItemWidget(
    SystemContext* systemContext,
    WindowContext* windowContext,
    QnWorkbenchItem* item,
    QGraphicsItem* parent)
    :
    base_type(systemContext, windowContext, item, parent),
    m_previewPainter(new ui::LayoutPreviewPainter())
{
    setPaletteColor(this, QPalette::Highlight, core::colorTheme()->color("brand_core"));
    setPaletteColor(this, QPalette::Dark, core::colorTheme()->color("dark17"));
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

ShowreelItemWidget::~ShowreelItemWidget()
{
}

Qn::ResourceStatusOverlay ShowreelItemWidget::calculateStatusOverlay() const
{
    return Qn::EmptyOverlay;
}

int ShowreelItemWidget::calculateButtonsVisibility() const
{
    return Qn::CloseButton;
}

void ShowreelItemWidget::initOverlay()
{
    auto font = this->font();
    font.setPixelSize(14);
    setFont(font);

    auto titleFont(font);
    titleFont.setWeight(QFont::Medium);

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
    const auto closeButtonIcon = qnSkin->icon("text_buttons/cross_close_20.svg", kIconSubstitutions);
    const auto closeButtonSize = core::Skin::maximumSize(closeButtonIcon);
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
            if (role != Qn::ShowreelItemOrderRole)
                return;

            const int order = item()->data(role).toInt();
            orderLabel->setText(QString::number(order));
        };
    updateOrder(Qn::ShowreelItemOrderRole);
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
                && item()->layout()->data(Qn::ShowreelIsManualRole).toBool();
            QColor textColor = palette().color(QPalette::Dark);
            QString text = isManual
                ? tr("Switch by", "Arrows will follow")
                    + QString(" %1 %2").arg(
                        nx::UnicodeChars::kLeftwardsArrow, nx::UnicodeChars::kRightwardsArrow)
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
    const auto delayMs = item()->data(Qn::ShowreelItemDelayMsRole).toInt();
    delayEdit->setValue(delayMs / 1000);

    connect(delayEdit, QnSpinboxIntValueChanged, this,
        [this](int value)
        {
            if (m_updating)
                return;

            QScopedValueRollback<bool> guard(m_updating, true);
            item()->setData(Qn::ShowreelItemDelayMsRole, value * 1000);

            // Store visual data to the tour and then add it to the save queue.
            menu()->trigger(menu::SaveCurrentShowreelAction);
        });

    connect(layoutResource().get(),
        &LayoutResource::itemDataChanged,
        this,
        [this, delayEdit](const QnUuid& id, Qn::ItemDataRole role, const QVariant& data)
        {
            if (m_updating || role != Qn::ShowreelItemDelayMsRole || id != item()->uuid())
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
            const bool isManual = item()->layout()->data(Qn::ShowreelIsManualRole).toBool();
            delayWidget->setVisible(!isManual);
        };

    connect(item()->layout(), &QnWorkbenchLayout::dataChanged, this,
        [updateManualMode, updateHint](int role)
        {
            if (role == Qn::ShowreelIsManualRole)
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

} // namespace nx::vms::client::desktop
