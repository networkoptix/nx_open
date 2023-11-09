// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "resource_widget.h"

#include <cassert>
#include <cmath>

#include <QtGui/QPainter>
#include <QtWidgets/QGraphicsLinearLayout>
#include <QtWidgets/QGraphicsScene>
#include <QtWidgets/QGraphicsView>
#include <QtWidgets/private/qpixmapfilter_p.h>

#include <qt_graphics_items/graphics_label.h>

#include <api/runtime_info_manager.h>
#include <client/client_module.h>
#include <client/client_runtime_settings.h>
#include <client/client_settings.h>
#include <core/resource/resource_media_layout.h>
#include <core/resource/security_cam_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/utils/range_adapters.h>
#include <nx/utils/string.h>
#include <nx/vms/client/core/utils/geometry.h>
#include <nx/vms/client/desktop/common/utils/painter_transform_scale_stripper.h>
#include <nx/vms/client/desktop/ini.h>
#include <nx/vms/client/desktop/resource/layout_resource.h>
#include <nx/vms/client/desktop/resource/resource_access_manager.h>
#include <nx/vms/client/desktop/statistics/context_statistics_module.h>
#include <nx/vms/client/desktop/style/skin.h>
#include <nx/vms/client/desktop/style/style.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/ui/common/color_theme.h>
#include <nx/vms/client/desktop/ui/graphics/items/overlays/selection_overlay_widget.h>
#include <nx/vms/client/desktop/videowall/videowall_online_screens_watcher.h>
#include <nx/vms/client/desktop/workbench/workbench.h>
#include <nx/vms/common/html/html.h>
#include <nx/vms/common/license/license_usage_watcher.h>
#include <nx/vms/common/system_context.h>
#include <nx/vms/license/usage_helper.h>
#include <ui/animation/opacity_animator.h>
#include <ui/common/cursor_cache.h>
#include <ui/common/palette.h>
#include <ui/graphics/items/controls/html_text_item.h>
#include <ui/graphics/items/generic/image_button_bar.h>
#include <ui/graphics/items/generic/image_button_widget.h>
#include <ui/graphics/items/generic/proxy_label.h>
#include <ui/graphics/items/generic/viewport_bound_widget.h>
#include <ui/graphics/items/overlays/hud_overlay_widget.h>
#include <ui/graphics/items/overlays/resource_status_overlay_widget.h>
#include <ui/graphics/items/overlays/resource_title_item.h>
#include <ui/graphics/items/overlays/status_overlay_controller.h>
#include <ui/graphics/items/resource/button_ids.h>
#include <ui/graphics/opengl/gl_context_data.h>
#include <ui/graphics/opengl/gl_shortcuts.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>
#include <ui/statistics/modules/controls_statistics_module.h>
#include <ui/workbench/workbench_access_controller.h>
#include <ui/workbench/workbench_display.h>
#include <ui/workbench/workbench_item.h>
#include <ui/workbench/workbench_layout.h>
#include <utils/common/aspect_ratio.h>
#include <utils/common/delayed.h>
#include <utils/common/scoped_painter_rollback.h>
#include <utils/common/synctime.h>
#include <utils/common/util.h>
#include <utils/math/color_transformations.h>
#include <utils/math/linear_combination.h>

using namespace nx::vms::client::desktop;
using nx::vms::client::core::Geometry;

namespace {

static constexpr QSize kTitleButtonSize(28, 28);

static const qreal kHudMargin = 4.0;

/** Default timeout before the video is displayed as "loading", in milliseconds. */
#ifdef QN_RESOURCE_WIDGET_FLASHY_LOADING_OVERLAY
const qint64 defaultLoadingTimeoutMSec = MAX_FRAME_DURATION_MS;
#else
const qint64 defaultLoadingTimeoutMSec = MAX_FRAME_DURATION_MS * 3;
#endif

/** Background color for overlay panels. */

const QColor overlayTextColor = QColor(255, 255, 255); // TODO: #sivanov Move to skin.

const float noAspectRatio = -1.0;

// TODO: #sivanov get rid of this
QnResourceVideoLayoutPtr qn_resourceWidget_defaultContentLayout(new QnDefaultResourceVideoLayout());

void splitFormat(const QString &format, QString *left, QString *right)
{
    int index = format.indexOf(QLatin1Char('\t'));
    if (index != -1)
    {
        *left = format.mid(0, index);
        *right = format.mid(index + 1);
    }
    else
    {
        *left = format;
        *right = QString();
    }
}

} // anonymous namespace

struct QnResourceWidget::Private
{
    Private(QnWorkbenchItem* item):
        layout(item->layout()->resource())
    {
        NX_ASSERT(layout, "Widget is created without layout");
    }

    LayoutResourcePtr layout;
};

// -------------------------------------------------------------------------- //
// Logic
// -------------------------------------------------------------------------- //
QnResourceWidget::QnResourceWidget(
    SystemContext* systemContext,
    WindowContext* windowContext,
    QnWorkbenchItem* item,
    QGraphicsItem* parent)
    :
    base_type(parent),
    SystemContextAware(systemContext),
    WindowContextAware(windowContext),
    m_hudOverlay(new QnHudOverlayWidget(this)),
    m_statusOverlay(new QnStatusOverlayWidget(this)),
    d(new Private(item)),
    m_item(item),
    m_options(DisplaySelection | AllowFocus),
    m_localActive(false),
    m_frameOpacity(1.0),
    m_titleTextFormat("%1"),
    m_titleTextFormatHasPlaceholder(true),
    m_mouseInWidget(false),
    m_renderStatus(Qn::NothingRendered),
    m_lastNewFrameTimeMSec(0),
    m_selectionState(SelectionState::invalid)
{
    updateSelectedState();

    setAcceptHoverEvents(true);
    setTransformOrigin(Center);

    /* Initialize resource. */
    m_resource = item->resource();
    connect(m_resource.get(), &QnResource::nameChanged, this, &QnResourceWidget::updateTitleText);
    connect(m_resource.get(), &QnResource::statusChanged, this,
        [this]
        {
            const bool animate = display()->animationAllowed();
            updateStatusOverlay(animate);
        });

    /* Set up overlay widgets. */
    QFont font = this->font();
    font.setStyleName("Arial");
    font.setPixelSize(15);
    setFont(font);

    setPaletteColor(this, QPalette::WindowText, overlayTextColor);
    setPaletteColor(this, QPalette::Window, colorTheme()->color("dark5"));
    setPaletteColor(this, QPalette::Highlight, colorTheme()->color("brand_core", 110));

    setupHud();
    setupSelectionOverlay();
    createButtons();

    executeLater([this]() { updateHud(false); }, this);

    /* Status overlay. */
    m_statusController = new QnStatusOverlayController(m_resource, m_statusOverlay, this);

    setupOverlayButtonsHandlers();

    addOverlayWidget(m_statusOverlay, {Invisible, OverlayFlag::autoRotate, StatusLayer});

    setChannelLayout(qn_resourceWidget_defaultContentLayout);

    m_aspectRatio = defaultAspectRatio();

    connect(d->layout.get(),
        &LayoutResource::itemDataChanged,
        this,
        [this, itemId = item->uuid()](
            const QnUuid& id, Qn::ItemDataRole role, const QVariant& /*data*/)
        {
            if (id != itemId)
                return;

            at_itemDataChanged(role);
        });
    connect(item, &QnWorkbenchItem::dataChanged, this, &QnResourceWidget::at_itemDataChanged);

    /* Videowall license changes helper */

    auto updateOverlay =
        [this]
        {
            const bool animate = display()->animationAllowed();
            updateStatusOverlay(animate);
        };

    // License Usage Watcher may be absent in cross-system contexts.
    if (auto licenseWatcher = m_resource->systemContext()->videoWallLicenseUsageWatcher())
    {
        connect(licenseWatcher,
            &nx::vms::common::LicenseUsageWatcher::licenseUsageChanged,
            this,
            updateOverlay);
    }

    if (qnRuntime->isVideoWallMode())
    {
        auto videoWallOnlineScreensWatcher = systemContext->videoWallOnlineScreensWatcher();
        connect(
            videoWallOnlineScreensWatcher,
            &VideoWallOnlineScreensWatcher::onlineScreensChanged,
            this,
            updateOverlay);
    }

    /* Run handlers. */
    setInfoVisible(titleBar()->rightButtonsBar()->button(Qn::InfoButton)->isChecked(), false);
    updateTitleText();
    updateButtonsVisibility();

    connect(this, &QnResourceWidget::rotationChanged, this, [this]()
    {
        if (m_enclosingGeometry.isValid())
            setGeometry(calculateGeometry(m_enclosingGeometry));
    });

    if (const auto layout = m_item->layout())
    {
        if (const auto layoutResource = layout->resource())
        {
            connect(layoutResource.get(), &QnLayoutResource::lockedChanged, this,
                [this](const QnLayoutResourcePtr&)
                {
                    updateButtonsVisibility();
                });
        }
    }
}

QnResourceWidget::~QnResourceWidget()
{
}

void QnResourceWidget::setupOverlayButtonsHandlers()
{
    const auto updateButtons =
        [this]()
        {
            updateOverlayButton();
            updateCustomOverlayButton();
        };

    connect(m_resource.get(), &QnResource::statusChanged, this, updateButtons);
    connect(m_statusController, &QnStatusOverlayController::statusOverlayChanged, this,
        [this, updateButtons](bool animated)
        {
            const auto visibility = (m_statusController->statusOverlay() == Qn::EmptyOverlay)
                ? Invisible
                : Visible;
            setOverlayWidgetVisibility(m_statusOverlay, visibility, animated);
            updateButtons();
        });

}

// TODO: #ynikitenkov #high emplace back "titleLayout->setContentsMargins(0, 0, 0, 1);" fix
void QnResourceWidget::setupHud()
{
    addOverlayWidget(m_hudOverlay, {UserVisible, OverlayFlag::autoRotate, InfoLayer});
    m_hudOverlay->setContentsMargins(0.0, 0.0, 0.0, 0.0);
    m_hudOverlay->content()->setContentsMargins(kHudMargin, 0.0, kHudMargin, kHudMargin);
    setOverlayWidgetVisible(m_hudOverlay, true, /*animate=*/false);
    setOverlayWidgetVisible(m_hudOverlay->details(), false, /*animate=*/false);
    setOverlayWidgetVisible(m_hudOverlay->position(), false, /*animate=*/false);
}

void QnResourceWidget::setupSelectionOverlay()
{
    addOverlayWidget(new ui::SelectionWidget(this), {Visible, OverlayFlag::none, SelectionLayer});
}

QIcon QnResourceWidget::loadSvgIcon(const QString& name) const
{
    static QHash<QString, QIcon> sIconCache;
    if (sIconCache.contains(name))
        return sIconCache[name];

    static const QMap<QIcon::Mode, QColor> colors = {
        {QIcon::Normal, colorTheme()->color("light4")},
        {QIcon::Active, colorTheme()->color("light1")},
        {QnIcon::Pressed, colorTheme()->color("light7")},
        {QIcon::Disabled, colorTheme()->color("light4", /*alpha*/ 77)}};

    static const QColor checkedBgColor = colorTheme()->color("brand_core");

    QPixmapDropShadowFilter filter;
    filter.setBlurRadius(2.5);
    filter.setOffset(QPointF(0, 0));
    filter.setColor(qRgba(0, 0, 0, 255));

    const auto preparePixmap =
        [&filter](const QPixmap& source, QColor background)
        {
            QPixmap result(kTitleButtonSize * source.devicePixelRatio());
            result.setDevicePixelRatio(source.devicePixelRatio());
            result.fill(Qt::transparent);

            QPainter painter(&result);
            if (background.isValid())
            {
                static constexpr qreal kRounding = 2.0;
                painter.setPen(Qt::NoPen);
                painter.setBrush(background);
                painter.drawRoundedRect(
                    QRect(QPoint(0, 0), kTitleButtonSize), kRounding, kRounding);
            }

            const auto sizeDiff = kTitleButtonSize - source.size() / source.devicePixelRatio();
            filter.draw(&painter, {sizeDiff.width() / 2.0, sizeDiff.height() / 2.0}, source);

            return result;
        };

    QIcon icon;
    const QPixmap pixmap = qnSkin->pixmap(name);

    for (const auto [mode, color]: nx::utils::constKeyValueRange(colors))
    {
        const auto colorized = Skin::colorize(pixmap, color);
        icon.addPixmap(preparePixmap(colorized, {}), mode, QIcon::Off);
        icon.addPixmap(preparePixmap(colorized, checkedBgColor), mode, QIcon::On);
    }

    sIconCache.insert(name, icon);
    return icon;
}

void QnResourceWidget::createButtons()
{
    auto leftButtonsBar = titleBar()->leftButtonsBar();
    leftButtonsBar->setUniformButtonSize(kTitleButtonSize);

    auto closeButton = createStatisticAwareButton("res_widget_close");
    closeButton->setIcon(loadSvgIcon("item/close.svg"));
    closeButton->setToolTip(tooltipText(tr("Close"), QKeySequence{"Del"}));
    closeButton->setObjectName("CloseButton");
    connect(closeButton, &QnImageButtonWidget::clicked, this, &QnResourceWidget::close,
        Qt::QueuedConnection);

    auto infoButton = createStatisticAwareButton("res_widget_info");
    infoButton->setIcon(loadSvgIcon("item/info.svg"));
    infoButton->setCheckable(true);
    infoButton->setChecked(item()->displayInfo());
    infoButton->setToolTip(tooltipText(tr("Information"), QKeySequence{"I"}));
    infoButton->setObjectName("InformationButton");
    connect(infoButton, &QnImageButtonWidget::toggled, this, &QnResourceWidget::at_infoButton_toggled);

    auto rotateButton = createStatisticAwareButton("res_widget_rotate");
    rotateButton->setIcon(loadSvgIcon("item/rotate.svg"));
    rotateButton->setToolTip(tooltipText(tr("Rotate"), "Alt+drag"));
    rotateButton->setObjectName("RotateButton");
    setHelpTopic(rotateButton, Qn::MainWindow_MediaItem_Rotate_Help);
    connect(rotateButton, &QnImageButtonWidget::pressed, this, &QnResourceWidget::rotationStartRequested);
    connect(rotateButton, &QnImageButtonWidget::released, this, &QnResourceWidget::rotationStopRequested);

    auto rightButtonsBar = titleBar()->rightButtonsBar();
    rightButtonsBar->setUniformButtonSize(kTitleButtonSize);
    rightButtonsBar->addButton(Qn::CloseButton, closeButton);
    rightButtonsBar->addButton(Qn::InfoButton, infoButton);
    rightButtonsBar->addButton(Qn::RotateButton, rotateButton);

    connect(
        rightButtonsBar,
        &QnImageButtonBar::checkedButtonsChanged,
        this,
        &QnResourceWidget::at_buttonBar_checkedButtonsChanged);

    auto iconButton = new QnImageButtonWidget();
    iconButton->setParent(this);
    iconButton->setPreferredSize(kTitleButtonSize);
    iconButton->setVisible(false);
    iconButton->setAcceptedMouseButtons(Qt::NoButton);
    iconButton->setObjectName("IconButton");
    titleBar()->leftButtonsBar()->addButton(Qn::RecordingStatusIconButton, iconButton);

    auto fullscreenButton = createStatisticAwareButton("web_widget_fullscreen");
    fullscreenButton->setObjectName("FullscreenButton");
    connect(
        fullscreenButton,
        &QnImageButtonWidget::clicked,
        this,
        [this]()
        {
          // Toggles fullscreen item mode.
          const auto newFullscreenItem = (options().testFlag(FullScreenMode) ? nullptr : item());
          workbench()->setItem(Qn::ZoomedRole, newFullscreenItem);
        }
    );
    rightButtonsBar->addButton(Qn::FullscreenButton, fullscreenButton);
    updateFullscreenButton();
}

void QnResourceWidget::updateFullscreenButton()
{
    const bool fullscreenMode = options().testFlag(FullScreenMode);
    const auto button = titleBar()->rightButtonsBar()->button(Qn::FullscreenButton);

    const auto newIcon = fullscreenMode
        ? loadSvgIcon("item/exit_fullscreen.svg")
        : loadSvgIcon("item/fullscreen.svg");

    button->setToolTip(fullscreenMode ? tr("Exit Fullscreen") : tr("Enter Fullscreen"));
    button->setIcon(newIcon);
}

const QnResourcePtr &QnResourceWidget::resource() const
{
    return m_resource;
}

LayoutResourcePtr QnResourceWidget::layoutResource() const
{
    return d->layout;
}

QnWorkbenchItem* QnResourceWidget::item() const
{
    return m_item.data();
}

const QRectF &QnResourceWidget::zoomRect() const
{
    return m_zoomRect;
}

void QnResourceWidget::setZoomRect(const QRectF &zoomRect)
{
    if (qFuzzyEquals(m_zoomRect, zoomRect))
        return;

    m_zoomRect = zoomRect;

    emit zoomRectChanged();
}

QnResourceWidget *QnResourceWidget::zoomTargetWidget() const
{
    return WindowContextAware::display()->zoomTargetWidget(const_cast<QnResourceWidget *>(this));
}

bool QnResourceWidget::isZoomWindow() const
{
    return !m_zoomRect.isNull();
}

qreal QnResourceWidget::frameOpacity() const
{
    return m_frameOpacity;
}

void QnResourceWidget::setFrameOpacity(qreal frameOpacity)
{
    m_frameOpacity = frameOpacity;
}

QColor QnResourceWidget::frameDistinctionColor() const
{
    return m_item->frameDistinctionColor();
}

void QnResourceWidget::setFrameDistinctionColor(const QColor &frameColor)
{
    m_item->setFrameDistinctionColor(frameColor);
}

float QnResourceWidget::aspectRatio() const
{
    return m_aspectRatio;
}

void QnResourceWidget::setAspectRatio(float aspectRatio)
{
    if (qFuzzyEquals(m_aspectRatio, aspectRatio))
        return;

    m_aspectRatio = aspectRatio;
    updateGeometry(); /* Discard cached size hints. */

    emit aspectRatioChanged();
}

bool QnResourceWidget::hasAspectRatio() const
{
    return m_aspectRatio > 0.0;
}

float QnResourceWidget::visualAspectRatio() const
{
    if (!hasAspectRatio())
        return -1;

    return QnAspectRatio::isRotated90(rotation()) ? 1 / m_aspectRatio : m_aspectRatio;
}

float QnResourceWidget::defaultVisualAspectRatio() const
{
    if (!m_enclosingGeometry.isNull())
        return m_enclosingGeometry.width() / m_enclosingGeometry.height();

    if (m_item && m_item->layout() && m_item->layout()->hasCellAspectRatio())
        return m_item->layout()->cellAspectRatio();

    return QnLayoutResource::kDefaultCellAspectRatio;
}

float QnResourceWidget::visualChannelAspectRatio() const
{
    if (!channelLayout())
        return visualAspectRatio();

    qreal layoutAspectRatio = Geometry::aspectRatio(channelLayout()->size());
    if (QnAspectRatio::isRotated90(rotation()))
        return visualAspectRatio() * layoutAspectRatio;
    else
        return visualAspectRatio() / layoutAspectRatio;
}

QRectF QnResourceWidget::enclosingGeometry() const
{
    return m_enclosingGeometry;
}

void QnResourceWidget::setEnclosingGeometry(const QRectF &enclosingGeometry, bool updateGeometry)
{
    m_enclosingGeometry = enclosingGeometry;
    if (updateGeometry)
        setGeometry(calculateGeometry(enclosingGeometry));
}

QRectF QnResourceWidget::calculateGeometry(const QRectF &enclosingGeometry, qreal rotation) const
{
    if (!enclosingGeometry.isEmpty())
    {
        /* Calculate bounds of the rotated item. */
        qreal aspectRatio = hasAspectRatio() ? m_aspectRatio : defaultVisualAspectRatio();
        return Geometry::encloseRotatedGeometry(enclosingGeometry, aspectRatio, rotation);
    }
    else
    {
        return enclosingGeometry;
    }
}

QRectF QnResourceWidget::calculateGeometry(const QRectF &enclosingGeometry) const
{
    return calculateGeometry(enclosingGeometry, this->rotation());
}

QnResourceWidget::Options QnResourceWidget::options() const
{
    return m_options;
}

QString QnResourceWidget::titleText() const
{
    return titleBar()->titleLabel()->text();
}

QString QnResourceWidget::titleTextFormat() const
{
    return m_titleTextFormat;
}

void QnResourceWidget::setTitleTextFormat(const QString &titleTextFormat)
{
    if (m_titleTextFormat == titleTextFormat)
        return;

    m_titleTextFormat = titleTextFormat;
    m_titleTextFormatHasPlaceholder = titleTextFormat.contains(QLatin1String("%1"));

    updateTitleText();
}

void QnResourceWidget::setTitleTextInternal(const QString &titleText)
{
    QString leftText, rightText;

    splitFormat(titleText, &leftText, &rightText);

    titleBar()->titleLabel()->setText(leftText);
    titleBar()->extraInfoLabel()->setText(rightText);
}

QString QnResourceWidget::calculateTitleText() const
{
    return m_resource->getName();
}

void QnResourceWidget::updateTitleText()
{
    setTitleTextInternal(m_titleTextFormatHasPlaceholder ? m_titleTextFormat.arg(calculateTitleText()) : m_titleTextFormat);
    // Set object name for debugging and testing purposes.
    setObjectName(m_titleTextFormatHasPlaceholder ? m_titleTextFormat.arg(m_resource->getName()) : m_titleTextFormat);
}

void QnResourceWidget::updateInfoText()
{
    updateDetailsText();
    updatePositionText();
}

QString QnResourceWidget::calculateDetailsText() const
{
    return QString();
}

QPixmap QnResourceWidget::calculateDetailsIcon() const
{
    return QPixmap();
}

void QnResourceWidget::updateDetailsText()
{
    if (!isOverlayWidgetVisible(m_hudOverlay->details()))
        return;

    const QString text = calculateDetailsText();
    m_hudOverlay->details()->setHtml(text);
    m_hudOverlay->details()->setIcon(calculateDetailsIcon());
    m_hudOverlay->details()->setVisible(!text.isEmpty());
}

QString QnResourceWidget::calculatePositionText() const
{
    return QString();
}

void QnResourceWidget::updatePositionText()
{
    if (!isOverlayWidgetVisible(m_hudOverlay->position()))
        return;

    const QString text = calculatePositionText();
    m_hudOverlay->position()->setHtml(text);
    m_hudOverlay->position()->setVisible(!text.isEmpty());
}

QnStatusOverlayController *QnResourceWidget::statusOverlayController() const
{
    return m_statusController;
}

QSizeF QnResourceWidget::constrainedSize(const QSizeF constraint, Qt::WindowFrameSection pinSection) const
{
    if (!hasAspectRatio())
        return constraint;

    QSizeF result = constraint;

    switch (pinSection)
    {
        case Qt::TopSection:
        case Qt::BottomSection:
            result.setWidth(constraint.height() * m_aspectRatio);
            break;
        case Qt::LeftSection:
        case Qt::RightSection:
            result.setHeight(constraint.width() / m_aspectRatio);
            break;
        default:
            result = Geometry::expanded(m_aspectRatio, constraint, Qt::KeepAspectRatioByExpanding);
            break;
    }

    return result;
}

void QnResourceWidget::updateCheckedButtons()
{
    if (!item())
        return;

    const auto checkedButtons = item()->data(Qn::ItemCheckedButtonsRole).toInt();
    titleBar()->rightButtonsBar()->setCheckedButtons(checkedButtons);
}

QVariant QnResourceWidget::itemChange(GraphicsItemChange change, const QVariant &value)
{
    if (change == QGraphicsItem::ItemSelectedHasChanged)
        updateSelectedState();

    return base_type::itemChange(change, value);
}

QString QnResourceWidget::tooltipText(const QString& toolTip, const QKeySequence& hotkey) const
{
    QString result(toolTip);
    if (!hotkey.isEmpty())
        result.append(" "
            + nx::vms::common::html::bold("(" + hotkey.toString(QKeySequence::NativeText) + ")"));

    return result;
}

QString QnResourceWidget::tooltipText(const QString& toolTip, const QString& hotkey) const
{
    // This overloaded function is used for non-standard hotkey sequences, such as "Alt+drag",
    // "Ctrl+click" and so on.
    QString result(toolTip);
    if (!hotkey.isEmpty())
    {
#if defined(Q_OS_MACX)
        static const int kShiftUnicode = 0x21E7;
        static const int kControlUnicode = 0x2303;
        static const int kOptionUnicode = 0x2325;
        static const int kCommandUnicode = 0x2318;

        QString extra(hotkey);
        extra.replace("Alt", QChar(kOptionUnicode));
        extra.replace("Ctrl", QChar(kCommandUnicode));
        extra.replace("Shift", QChar(kShiftUnicode));
        extra.replace("Meta", QChar(kControlUnicode));

        result.append(" " + nx::vms::common::html::bold("(" + extra + ")"));
#else
        result.append(" " + nx::vms::common::html::bold("(" + hotkey + ")"));
#endif
    }
    return result;
}

bool QnResourceWidget::isVideoWallLicenseValid() const
//TODO @pprivalov: code duplication with resource_status_helper
{
    auto helper = qnClientModule->videoWallLicenseUsageHelper();
    if (helper->isValid())
        return true;

    const QnPeerRuntimeInfo localInfo = m_resource->systemContext()->runtimeInfoManager()->localInfo();
    NX_ASSERT(localInfo.data.peer.peerType == nx::vms::api::PeerType::videowallClient);
    QnUuid currentScreenId = localInfo.data.videoWallInstanceGuid;

    // Gather all online screen ids.
    // The order of screens should be the same across all client instances.
    auto videoWallOnlineScreensWatcher = systemContext()->videoWallOnlineScreensWatcher();
    const auto onlineScreens = videoWallOnlineScreensWatcher->onlineScreens();

    const int allowedLiceses = helper->totalLicenses(Qn::LC_VideoWall);

    // Calculate the number of screens that should show invalid license overlay.
    using Helper = nx::vms::license::VideoWallLicenseUsageHelper;
    size_t screensToDisable = 0;
    while (screensToDisable < onlineScreens.size()
        && Helper::licensesForScreens(onlineScreens.size() - screensToDisable) > allowedLiceses)
    {
        ++screensToDisable;
    }

    if (screensToDisable > 0)
    {
        // If current screen falls into the range - report invalid license.
        const size_t i = std::distance(onlineScreens.begin(), onlineScreens.find(currentScreenId));
        if (i < screensToDisable)
            return false;
    }

    return true;
}

QnResourceWidget::SelectionState QnResourceWidget::selectionState() const
{
    return m_selectionState;
}

QPixmap QnResourceWidget::placeholderPixmap() const
{
    return m_placeholderPixmap;
}

void QnResourceWidget::setPlaceholderPixmap(const QPixmap& pixmap)
{
    m_placeholderPixmap = pixmap;
    emit placeholderPixmapChanged();
}

QString QnResourceWidget::actionText() const
{
    return m_hudOverlay->actionText();
}

void QnResourceWidget::setActionText(const QString& value)
{
    m_hudOverlay->setActionText(value);
}

void QnResourceWidget::clearActionText(std::chrono::milliseconds after)
{
    m_hudOverlay->clearActionText(after);
}

QnUuid QnResourceWidget::uuid() const
{
    return item() ? item()->uuid() : QnUuid();
}

QString QnResourceWidget::uuidString() const
{
    return uuid().toString();
}

QString QnResourceWidget::toString() const
{
    return nx::format("QnResourceWidget %1 (%2)").args(uuid(), m_resource);

}

QSizeF QnResourceWidget::sizeHint(Qt::SizeHint which, const QSizeF &constraint) const
{
    QSizeF result;
    switch (which)
    {
        case Qt::MinimumSize:
        {
            static const qreal kMinPartOfCell = 0.25;
            static const qreal kMinimalWidth = Workbench::kUnitSize * kMinPartOfCell;
            static const qreal kMinimalHeight = kMinimalWidth
                / QnLayoutResource::kDefaultCellAspectRatio;
            result = QSizeF(kMinimalWidth, kMinimalHeight);
            break;
        }
        case Qt::MaximumSize:
        {
            static const int kMaxCells = 64;
            static const qreal kMaximalWidth = Workbench::kUnitSize * kMaxCells;
            static const qreal kMaximalHeight = kMaximalWidth
                / QnLayoutResource::kDefaultCellAspectRatio;
            result = QSizeF(kMaximalWidth, kMaximalHeight);
            break;
        }

        default:
            result = base_type::sizeHint(which, constraint);
            break;
    }

    if (!hasAspectRatio())
        return result;

    if (which == Qt::MinimumSize)
        return Geometry::expanded(m_aspectRatio, result, Qt::KeepAspectRatioByExpanding);

    return result;
}

QRectF QnResourceWidget::channelRect(int channel) const
{
    /* Channel rect is handled at shader level if dewarping is enabled. */
    QRectF rect = ((m_options & DisplayDewarped) || zoomRect().isNull())
        ? this->rect()
        : Geometry::unsubRect(this->rect(), zoomRect());

    if (m_channelsLayout->channelCount() == 1)
        return rect;

    QSizeF channelSize = Geometry::cwiseDiv(rect.size(), m_channelsLayout->size());
    return QRectF(
        rect.topLeft() + Geometry::cwiseMul(m_channelsLayout->position(channel), channelSize),
        channelSize
    );
}

// TODO: #sivanov Remove useRelativeCoordinates.
QRectF QnResourceWidget::exposedRect(int channel, bool accountForViewport, bool accountForVisibility, bool useRelativeCoordinates)
{
    if (accountForVisibility && (!isVisible() || qFuzzyIsNull(effectiveOpacity())))
        return QRectF();

    QRectF channelRect = this->channelRect(channel);
    if (channelRect.isEmpty())
        return QRectF();

    QRectF result = channelRect.intersected(rect());
    if (result.isEmpty())
        return QRectF();

    if (accountForViewport)
    {
        if (scene()->views().empty())
            return QRectF();
        QGraphicsView *view = scene()->views()[0];

        QRectF viewportRect = mapRectFromScene(QnSceneTransformations::mapRectToScene(view, view->viewport()->rect()));
        result = result.intersected(viewportRect);
        if (result.isEmpty())
            return QRectF();
    }

    if (useRelativeCoordinates)
    {
        return Geometry::toSubRect(channelRect, result);
    }
    else
    {
        return result;
    }
}

void QnResourceWidget::registerButtonStatisticsAlias(QnImageButtonWidget* button, const QString& alias)
{
    statisticsModule()->controls()->registerButton(alias, button);
}

QnImageButtonWidget* QnResourceWidget::createStatisticAwareButton(const QString& alias)
{
    QnImageButtonWidget* result = new QnImageButtonWidget();
    registerButtonStatisticsAlias(result, alias);
    return result;
}

Qn::RenderStatus QnResourceWidget::renderStatus() const
{
    return m_renderStatus;
}

bool QnResourceWidget::isLocalActive() const
{
    return m_localActive;
}

void QnResourceWidget::setLocalActive(bool localActive)
{
    if (m_localActive == localActive)
        return;

    m_localActive = localActive;
    updateSelectedState();
}

QnResourceTitleItem* QnResourceWidget::titleBar() const
{
    return m_hudOverlay->title();
}

void QnResourceWidget::setCheckedButtons(int buttons)
{
    titleBar()->rightButtonsBar()->setCheckedButtons(buttons);
    titleBar()->leftButtonsBar()->setCheckedButtons(buttons);
}

int QnResourceWidget::checkedButtons() const
{
    return (titleBar()->rightButtonsBar()->checkedButtons()
            | titleBar()->leftButtonsBar()->checkedButtons());
}

int QnResourceWidget::visibleButtons() const
{
    return (titleBar()->rightButtonsBar()->visibleButtons()
            | titleBar()->leftButtonsBar()->visibleButtons());
}

int QnResourceWidget::calculateButtonsVisibility() const
{
    int result = Qn::InfoButton;

    const bool fullscreenMode = options().testFlag(FullScreenMode);

    const Qn::Permissions permissions = ResourceAccessManager::permissions(d->layout);
    if (!m_options.testFlag(WindowRotationForbidden) && permissions.testFlag(Qn::WritePermission))
        result |= Qn::RotateButton;

    const bool hasCloseButtonPermissions =
        permissions.testFlag(Qn::WritePermission)
        && permissions.testFlag(Qn::AddRemoveItemsPermission);

    if (hasCloseButtonPermissions && !fullscreenMode)
    {
         result |= Qn::CloseButton;
    }

    if (fullscreenMode)
        result |= Qn::FullscreenButton;

    return result;
}

void QnResourceWidget::updateButtonsVisibility()
{
    // TODO: #ynikitenkov Change destroying sequence: items should be destroyed before layout
    if (!item() || !item()->layout())
        return;

    const auto visibleButtons = calculateButtonsVisibility()
        & ~(item()->data<int>(Qn::ItemDisabledButtonsRole, 0));

    titleBar()->rightButtonsBar()->setVisibleButtons(visibleButtons);
    titleBar()->leftButtonsBar()->setVisibleButtons(visibleButtons);
}

int QnResourceWidget::helpTopicAt(const QPointF &) const
{
    return -1;
}

void QnResourceWidget::setOption(Option option, bool value /*= true*/)
{
    if (value && option == DisplayMotion)
        NX_ASSERT(m_resource && m_resource->hasFlags(Qn::motion));

    setOptions(Options(m_options).setFlag(option, value));
}

void QnResourceWidget::setOptions(Options options)
{
    if (m_options == options)
        return;

    Options changedOptions = m_options ^ options;
    m_options = options;

    optionsChangedNotify(changedOptions);
    emit optionsChanged(changedOptions, {});
}

const QSize &QnResourceWidget::channelScreenSize() const
{
    return m_channelScreenSize;
}

void QnResourceWidget::setChannelScreenSize(const QSize &size)
{
    if (size == m_channelScreenSize)
        return;

    m_channelScreenSize = size;

    channelScreenSizeChangedNotify();
}

bool QnResourceWidget::isInfoVisible() const
{
    return options().testFlag(DisplayInfo);
}

void QnResourceWidget::setInfoVisible(bool visible, bool animate)
{
    if (isInfoVisible() == visible)
        return;

    setOption(DisplayInfo, visible);
    item()->setDisplayInfo(visible);
    updateHud(animate);
}

Qn::ResourceStatusOverlay QnResourceWidget::calculateStatusOverlay(
    nx::vms::api::ResourceStatus resourceStatus, bool hasVideo) const
{
    if (ini().disableVideoRendering)
        return Qn::EmptyOverlay;

    if (resourceStatus == nx::vms::api::ResourceStatus::offline)
        return Qn::OfflineOverlay;

    if (resourceStatus == nx::vms::api::ResourceStatus::unauthorized)
        return Qn::UnauthorizedOverlay;

    if (m_renderStatus == Qn::NewFrameRendered)
        return Qn::EmptyOverlay;

    if (!hasVideo)
        return Qn::NoVideoDataOverlay;

    if (m_renderStatus == Qn::NothingRendered || m_renderStatus == Qn::CannotRender)
        return Qn::LoadingOverlay;

    if (QDateTime::currentMSecsSinceEpoch() - m_lastNewFrameTimeMSec >= defaultLoadingTimeoutMSec)
        return Qn::LoadingOverlay; //< m_renderStatus is OldFrameRendered at this point.

    return Qn::EmptyOverlay;
}

Qn::ResourceStatusOverlay QnResourceWidget::calculateStatusOverlay() const
{
    const auto mediaRes = m_resource.dynamicCast<QnMediaResource>();
    return calculateStatusOverlay(m_resource->getStatus(), mediaRes && mediaRes->hasVideo(0));
}

void QnResourceWidget::updateStatusOverlay(bool animate)
{
    m_statusController->setStatusOverlay(calculateStatusOverlay(), animate);
}

Qn::ResourceOverlayButton QnResourceWidget::calculateOverlayButton(
    Qn::ResourceStatusOverlay statusOverlay) const
{
    Q_UNUSED(statusOverlay);
    return Qn::ResourceOverlayButton::Empty;
}

QString QnResourceWidget::overlayCustomButtonText(Qn::ResourceStatusOverlay /*statusOverlay*/) const
{
    return QString();
}

void QnResourceWidget::updateOverlayButton()
{
    const auto statusOverlay = m_statusController->statusOverlay();
    m_statusController->setCurrentButton(calculateOverlayButton(statusOverlay));
}

void QnResourceWidget::updateCustomOverlayButton()
{
    const auto statusOverlay = m_statusController->statusOverlay();
    m_statusController->setCustomButtonText(overlayCustomButtonText(statusOverlay));
}

void QnResourceWidget::setChannelLayout(QnConstResourceVideoLayoutPtr channelLayout)
{
    if (m_channelsLayout == channelLayout)
        return;

    m_channelsLayout = channelLayout;

    channelLayoutChangedNotify();
}

int QnResourceWidget::channelCount() const
{
    return m_channelsLayout->channelCount();
}

bool QnResourceWidget::forceShowPosition() const
{
    return false;
}

void QnResourceWidget::updateHud(bool animate)
{
    /*
        Logic must be the following:
        * if widget is in full screen mode and there is no activity - hide all following overlays
        * otherwise, there are two options: 'mouse in the widget' and 'info button checked'
        * camera name should be visible if any option is active
        * position item should be visible if any option is active
        * control buttons should be visible if mouse cursor is in the widget
        * detailed info should be visible if both options are active simultaneously (or runtime property set)
    */

    /* Motion mask widget should not have overlays at all */

    const bool isInactiveInFullScreen = (options().testFlag(FullScreenMode)
                                   && !options().testFlag(ActivityPresence));

    const bool overlaysCanBeVisible = (!isInactiveInFullScreen
                                 && !options().testFlag(InfoOverlaysForbidden));

    const bool detailsVisible = m_options.testFlag(DisplayInfo);
    if (auto infoButton = titleBar()->rightButtonsBar()->button(Qn::InfoButton))
        infoButton->setChecked(detailsVisible);

    const bool alwaysShowName = m_options.testFlag(AlwaysShowName);

    const bool showOnlyCameraName = ((overlaysCanBeVisible && detailsVisible) || alwaysShowName)
        && !m_mouseInWidget;
    const bool showCameraNameWithButtons = overlaysCanBeVisible && m_mouseInWidget;
    const bool showPosition = (overlaysCanBeVisible && (detailsVisible || m_mouseInWidget))
        || forceShowPosition();
    const bool showDetailedInfo = overlaysCanBeVisible && detailsVisible &&
        (m_mouseInWidget || qnRuntime->showFullInfo() || qnSettings->showFullInfo());

    const bool showButtonsOverlay = (showOnlyCameraName || showCameraNameWithButtons);

    const bool updatePositionTextRequired = showPosition
        && !isOverlayWidgetVisible(m_hudOverlay->position());
    setOverlayWidgetVisible(m_hudOverlay->position(), showPosition, animate);
    if (updatePositionTextRequired)
        updatePositionText();

    const bool updateDetailsTextRequired = (showDetailedInfo && !isOverlayWidgetVisible(m_hudOverlay->details()));
    setOverlayWidgetVisible(m_hudOverlay->details(), showDetailedInfo, animate);
    if (updateDetailsTextRequired)
        updateDetailsText();

    setOverlayWidgetVisible(titleBar(), showButtonsOverlay, animate);
    titleBar()->setSimpleMode(showOnlyCameraName, animate);
}

bool QnResourceWidget::isHovered() const
{
    return m_mouseInWidget;
}

// -------------------------------------------------------------------------- //
// Painting
// -------------------------------------------------------------------------- //
void QnResourceWidget::paint(QPainter* painter,
    const QStyleOptionGraphicsItem* /*option*/,
    QWidget* /*widget*/)
{
    const bool animate = display()->animationAllowed();

    QnScopedPainterPenRollback penRollback(painter);
    QnScopedPainterBrushRollback brushRollback(painter);
    QnScopedPainterFontRollback fontRollback(painter);

    /* Update screen size of a single channel. */
    setChannelScreenSize(painter->combinedTransform().mapRect(channelRect(0)).size().toSize());

    Qn::RenderStatus renderStatus = Qn::NothingRendered;

    for (int i = 0; i < channelCount(); i++)
    {
        /* Draw content. */
        QRectF channelRect = this->channelRect(i);
        QRectF paintRect = this->exposedRect(i, false, false, false);
        if (paintRect.isEmpty())
            continue;

        renderStatus = qMax(renderStatus, paintChannelBackground(painter, i, channelRect, paintRect));

        /* Draw foreground. */
        paintChannelForeground(painter, i, paintRect);
    }

    paintEffects(painter);

    /* Update overlay. */
    m_renderStatus = renderStatus;
    if (renderStatus == Qn::NewFrameRendered)
        m_lastNewFrameTimeMSec = QDateTime::currentMSecsSinceEpoch();
    updateStatusOverlay(animate);
    emit painted();
}

void QnResourceWidget::paintEffects(QPainter* /*painter*/)
{
}

void QnResourceWidget::paintWindowFrame(QPainter* /*painter*/,
    const QStyleOptionGraphicsItem* /*option*/,
    QWidget* /*widget*/)
{
    // Skip standard implementation.
}

void QnResourceWidget::paintChannelForeground(QPainter *, int, const QRectF &)
{
    return;
}

void QnResourceWidget::updateSelectedState()
{
    const auto calculateSelectedState =
        [this]()
        {
            const bool selected = isSelected();
            const bool focused = options().testFlag(AllowFocus) && isSelected();
            const bool active = options().testFlag(AllowFocus) && isLocalActive();

            if (selected && focused && active)
                return SelectionState::focusedAndSelected;
            else if (!active && selected)
                return SelectionState::selected;
            else if (focused)
                return SelectionState::focused;
            else if (active)
                return SelectionState::inactiveFocused;

            return SelectionState::notSelected;
        };

    const auto selectionState = calculateSelectedState();
    if (selectionState == m_selectionState)
        return;

    m_selectionState = selectionState;
    emit selectionStateChanged(m_selectionState, {});
}

Qn::RenderStatus QnResourceWidget::paintChannelBackground(QPainter* painter, int /*channel*/,
    const QRectF& /*channelRect*/, const QRectF& paintRect)
{
    const PainterTransformScaleStripper scaleStripper(painter);
    painter->fillRect(scaleStripper.mapRect(paintRect), palette().color(QPalette::Window));
    return Qn::NewFrameRendered;
}

float QnResourceWidget::defaultAspectRatio() const
{
    if (item())
        return item()->data(Qn::ItemAspectRatioRole, noAspectRatio);
    return noAspectRatio;
}


// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //
void QnResourceWidget::hoverEnterEvent(QGraphicsSceneHoverEvent *event)
{
    m_mouseInWidget = true;

    const bool animate = display()->animationAllowed();

    setOverlayVisible(true, animate);
    updateHud(animate);
    base_type::hoverEnterEvent(event);
}

void QnResourceWidget::hoverMoveEvent(QGraphicsSceneHoverEvent *event)
{
    if (!m_mouseInWidget)
        hoverEnterEvent(event);

    const bool animate = display()->animationAllowed();

    setOverlayVisible(true, animate);
    base_type::hoverMoveEvent(event);
}

void QnResourceWidget::hoverLeaveEvent(QGraphicsSceneHoverEvent *event)
{
    m_mouseInWidget = false;
    const bool animate = display()->animationAllowed();

    setOverlayVisible(false, animate);
    updateHud(animate);
    base_type::hoverLeaveEvent(event);
}

void QnResourceWidget::optionsChangedNotify(Options changedFlags)
{
    const bool animate = display()->animationAllowed();
    const auto rightButtonsBar = titleBar()->rightButtonsBar();
    const auto visibleButtons = rightButtonsBar->visibleButtons();
    const bool infoButtonVisible = (visibleButtons & Qn::InfoButton);
    const bool updateHudWoAnimation =
        (changedFlags.testFlag(DisplayInfo) && infoButtonVisible)
        || (changedFlags.testFlag(InfoOverlaysForbidden));

    if (updateHudWoAnimation)
    {
        updateHud(false);
        return;
    }

    if (changedFlags.testFlag(ActivityPresence))
        updateHud(animate);

    if (changedFlags.testFlag(DisplaySelection))
        updateSelectedState();

    if (changedFlags.testFlag(FullScreenMode))
    {
        updateHud(true);
        updateFullscreenButton();
    }

    updateButtonsVisibility();
}

void QnResourceWidget::at_itemDataChanged(int role)
{
    if (role != Qn::ItemCheckedButtonsRole)
        return;

    updateCheckedButtons();
}

void QnResourceWidget::at_infoButton_toggled(bool toggled)
{
    const bool animate = display()->animationAllowed();

    setInfoVisible(toggled, animate);
}

void QnResourceWidget::at_buttonBar_checkedButtonsChanged()
{
    if (!item())
        return;

    const auto checkedButtons = titleBar()->rightButtonsBar()->checkedButtons();
    item()->setData(Qn::ItemCheckedButtonsRole, checkedButtons);
    update();
}
