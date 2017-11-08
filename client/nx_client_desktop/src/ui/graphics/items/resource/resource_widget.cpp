#include "resource_widget.h"

#include <cassert>
#include <cmath>

#include <QtGui/QPainter>
#include <QtWidgets/QGraphicsScene>
#include <QtWidgets/QGraphicsView>
#include <QtWidgets/QGraphicsLinearLayout>

#include <client/client_settings.h>
#include <client/client_runtime_settings.h>

#include <utils/common/aspect_ratio.h>
#include <utils/common/delayed.h>
#include <utils/common/scoped_painter_rollback.h>
#include <utils/common/synctime.h>
#include <utils/common/util.h>
#include <utils/common/warnings.h>
#include <utils/license_usage_helper.h>
#include <utils/math/color_transformations.h>
#include <utils/math/linear_combination.h>

#include <core/resource/resource_media_layout.h>
#include <core/resource/security_cam_resource.h>
#include <core/resource/layout_resource.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource_management/resource_runtime_data.h>

#include <nx/client/desktop/ui/common/painter_transform_scale_stripper.h>
#include <nx/client/desktop/ui/graphics/items/overlays/selection_overlay_widget.h>
#include <ui/common/cursor_cache.h>
#include <ui/common/palette.h>
#include <ui/help/help_topics.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/animation/opacity_animator.h>
#include <ui/graphics/opengl/gl_shortcuts.h>
#include <ui/graphics/opengl/gl_context_data.h>
#include <ui/graphics/items/resource/button_ids.h>
#include <ui/graphics/items/controls/html_text_item.h>
#include <ui/graphics/items/standard/graphics_label.h>
#include <ui/graphics/items/generic/proxy_label.h>
#include <ui/graphics/items/generic/image_button_widget.h>
#include <ui/graphics/items/generic/image_button_bar.h>
#include <ui/graphics/items/generic/viewport_bound_widget.h>
#include <ui/graphics/items/overlays/resource_title_item.h>
#include <ui/graphics/items/overlays/resource_status_overlay_widget.h>
#include <ui/graphics/items/overlays/status_overlay_controller.h>
#include <ui/graphics/items/overlays/hud_overlay_widget.h>
#include <ui/statistics/modules/controls_statistics_module.h>
#include <ui/workbench/workbench.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_item.h>
#include <ui/workbench/workbench_layout.h>
#include <ui/workbench/workbench_display.h>
#include <ui/workbench/workbench_access_controller.h>
#include <ui/style/globals.h>
#include <ui/style/skin.h>
#include <ui/style/nx_style.h>
#include <nx/utils/string.h>

using namespace nx::client::desktop::ui;

namespace {
const qreal kButtonsSize = 24.0;

static const qreal kHudMargin = 4.0;

/** Default timeout before the video is displayed as "loading", in milliseconds. */
#ifdef QN_RESOURCE_WIDGET_FLASHY_LOADING_OVERLAY
const qint64 defaultLoadingTimeoutMSec = MAX_FRAME_DURATION;
#else
const qint64 defaultLoadingTimeoutMSec = MAX_FRAME_DURATION * 3;
#endif

/** Background color for overlay panels. */

const QColor overlayTextColor = QColor(255, 255, 255); // TODO: #gdm #customization

const float noAspectRatio = -1.0;

//Q_GLOBAL_STATIC(QnDefaultResourceVideoLayout, qn_resourceWidget_defaultContentLayout);
QSharedPointer<QnDefaultResourceVideoLayout> qn_resourceWidget_defaultContentLayout(new QnDefaultResourceVideoLayout()); // TODO: #Elric get rid of this

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

bool itemBelongsToValidLayout(QnWorkbenchItem *item)
{
    return (item
            && item->layout()
            && item->layout()->resource()
            && item->layout()->resource()->resourcePool()
            && !item->layout()->resource()->getParentId().isNull());
}


} // anonymous namespace

// -------------------------------------------------------------------------- //
// Logic
// -------------------------------------------------------------------------- //
QnResourceWidget::QnResourceWidget(QnWorkbenchContext *context, QnWorkbenchItem *item, QGraphicsItem *parent) :
    base_type(parent),
    QnWorkbenchContextAware(context),
    m_hudOverlay(new QnHudOverlayWidget(this)),
    m_statusOverlay(new QnStatusOverlayWidget(this)),
    m_item(item),
    m_options(DisplaySelection),
    m_localActive(false),
    m_frameOpacity(1.0),
    m_frameDistinctionColor(),
    m_titleTextFormat(lit("%1")),
    m_titleTextFormatHasPlaceholder(true),
    m_aboutToBeDestroyedEmitted(false),
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
    connect(m_resource, &QnResource::nameChanged, this, &QnResourceWidget::updateTitleText);
    connect(m_resource, &QnResource::statusChanged, this,
        [this]
        {
            const bool animate = display()->animationAllowed();
            updateStatusOverlay(animate);
        });

    /* Set up overlay widgets. */
    QFont font = this->font();
    font.setStyleName(lit("Arial"));
    font.setPixelSize(15);
    setFont(font);

    setPaletteColor(this, QPalette::WindowText, overlayTextColor);

    setupHud();
    setupSelectionOverlay();
    createButtons();

    executeDelayedParented([this]() { updateHud(false); }, 0, this);

    /* Handle layout permissions if an item is placed on the common layout. Otherwise, it can be Motion Widget, for example. */
    if (itemBelongsToValidLayout(item))
        connect(accessController()->notifier(item->layout()->resource()), &QnWorkbenchPermissionsNotifier::permissionsChanged, this, &QnResourceWidget::updateButtonsVisibility);

    /* Status overlay. */
    m_statusController = new QnStatusOverlayController(m_resource, m_statusOverlay, this);

    setupOverlayButtonsHandlers();

    addOverlayWidget(m_statusOverlay, detail::OverlayParams(Invisible, true, false, StatusLayer));

    setChannelLayout(qn_resourceWidget_defaultContentLayout);

    m_aspectRatio = defaultAspectRatio();

    connect(qnResourceRuntimeDataManager, &QnResourceRuntimeDataManager::layoutItemDataChanged,
        this, [this, itemId = item->uuid()](
            const QnUuid& id, Qn::ItemDataRole role, const QVariant& /*data*/)
        {
            if (id != itemId)
                return;
            at_itemDataChanged(role);
        });
    connect(item, &QnWorkbenchItem::dataChanged, this, &QnResourceWidget::at_itemDataChanged);

    /* Videowall license changes helper */
    auto videowallLicenseHelper = new QnVideoWallLicenseUsageWatcher(commonModule(), this);
    connect(videowallLicenseHelper, &QnLicenseUsageWatcher::licenseUsageChanged, this,
        [this]
        {
            const bool animate = display()->animationAllowed();
            updateStatusOverlay(animate);
        });

    /* Run handlers. */
    setInfoVisible(titleBar()->rightButtonsBar()->button(Qn::InfoButton)->isChecked(), false);
    updateTitleText();
    updateButtonsVisibility();

    connect(this, &QnResourceWidget::rotationChanged, this, [this]()
    {
        if (m_enclosingGeometry.isValid())
            setGeometry(calculateGeometry(m_enclosingGeometry));
    });
}

QnResourceWidget::~QnResourceWidget()
{
    ensureAboutToBeDestroyedEmitted();
}

void QnResourceWidget::setupOverlayButtonsHandlers()
{
    const auto updateButtons =
        [this]()
        {
            updateOverlayButton();
            updateCustomOverlayButton();
        };

    connect(m_resource, &QnResource::statusChanged, this, updateButtons);
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
    addOverlayWidget(m_hudOverlay, detail::OverlayParams(UserVisible, true, false, InfoLayer));
    m_hudOverlay->setContentsMargins(0.0, 0.0, 0.0, 0.0);
    m_hudOverlay->content()->setContentsMargins(kHudMargin, 0.0, kHudMargin, kHudMargin);
    setOverlayWidgetVisible(m_hudOverlay, true, /*animate=*/false);
    setOverlayWidgetVisible(m_hudOverlay->details(), false, /*animate=*/false);
    setOverlayWidgetVisible(m_hudOverlay->position(), false, /*animate=*/false);
}

void QnResourceWidget::setupSelectionOverlay()
{
    addOverlayWidget(new SelectionWidget(this), {Visible, false, false, SelectionLayer});
}

void QnResourceWidget::createButtons()
{
    auto closeButton = createStatisticAwareButton(lit("res_widget_close"));
    closeButton->setIcon(qnSkin->icon("item/close.png"));
    closeButton->setToolTip(tr("Close"));
    connect(closeButton, &QnImageButtonWidget::clicked, this, &QnResourceWidget::close);

    auto infoButton = createStatisticAwareButton(lit("res_widget_info"));
    infoButton->setIcon(qnSkin->icon("item/info.png"));
    infoButton->setCheckable(true);
    infoButton->setChecked(item()->displayInfo());
    infoButton->setToolTip(tr("Information"));
    connect(infoButton, &QnImageButtonWidget::toggled, this, &QnResourceWidget::at_infoButton_toggled);

    auto rotateButton = createStatisticAwareButton(lit("res_widget_rotate"));
    rotateButton->setIcon(qnSkin->icon("item/rotate.png"));
    rotateButton->setToolTip(tr("Rotate"));
    setHelpTopic(rotateButton, Qn::MainWindow_MediaItem_Rotate_Help);
    connect(rotateButton, &QnImageButtonWidget::pressed, this, &QnResourceWidget::rotationStartRequested);
    connect(rotateButton, &QnImageButtonWidget::released, this, &QnResourceWidget::rotationStopRequested);

    auto buttonsBar = titleBar()->rightButtonsBar();
    buttonsBar->setUniformButtonSize(QSizeF(kButtonsSize, kButtonsSize));
    buttonsBar->addButton(Qn::CloseButton, closeButton);
    buttonsBar->addButton(Qn::InfoButton, infoButton);
    buttonsBar->addButton(Qn::RotateButton, rotateButton);

    connect(buttonsBar, SIGNAL(checkedButtonsChanged()), this, SLOT(at_buttonBar_checkedButtonsChanged()));

    auto iconButton = new QnImageButtonWidget();
    iconButton->setParent(this);
    iconButton->setPreferredSize(kButtonsSize, kButtonsSize);
    iconButton->setVisible(false);
    iconButton->setAcceptedMouseButtons(Qt::NoButton);
    titleBar()->leftButtonsBar()->addButton(Qn::RecordingStatusIconButton, iconButton);
}

const QnResourcePtr &QnResourceWidget::resource() const
{
    return m_resource;
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
    return QnWorkbenchContextAware::display()->zoomTargetWidget(const_cast<QnResourceWidget *>(this));
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
    return m_frameDistinctionColor;
}

void QnResourceWidget::setFrameDistinctionColor(const QColor &frameColor)
{
    if (m_frameDistinctionColor == frameColor)
        return;

    m_frameDistinctionColor = frameColor;

    emit frameDistinctionColorChanged();
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

    return qnGlobals->defaultLayoutCellAspectRatio();
}

float QnResourceWidget::visualChannelAspectRatio() const
{
    if (!channelLayout())
        return visualAspectRatio();

    qreal layoutAspectRatio = QnGeometry::aspectRatio(channelLayout()->size());
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
        return encloseRotatedGeometry(enclosingGeometry, aspectRatio, rotation);
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
    enum
    {
        kMaxNameLength = 30
    };

    return nx::utils::elideString(m_resource->getName(), kMaxNameLength);
}

void QnResourceWidget::updateTitleText()
{
    setTitleTextInternal(m_titleTextFormatHasPlaceholder ? m_titleTextFormat.arg(calculateTitleText()) : m_titleTextFormat);
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

void QnResourceWidget::updateDetailsText()
{
    if (!isOverlayWidgetVisible(m_hudOverlay->details()))
        return;

    const QString text = calculateDetailsText();
    m_hudOverlay->details()->setHtml(text);
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
            result = expanded(m_aspectRatio, constraint, Qt::KeepAspectRatioByExpanding);
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

QSizeF QnResourceWidget::sizeHint(Qt::SizeHint which, const QSizeF &constraint) const
{
    QSizeF result;
    switch (which)
    {
        case Qt::MinimumSize:
        {
            static const qreal kMinPartOfCell = 0.25;
            static const qreal kMinimalWidth = qnGlobals->workbenchUnitSize() * kMinPartOfCell;
            static const qreal kMinimalHeight = kMinimalWidth
                / qnGlobals->defaultLayoutCellAspectRatio();
            result = QSizeF(kMinimalWidth, kMinimalHeight);
            break;
        }
        case Qt::MaximumSize:
        {
            static const int kMaxCells = 64;
            static const qreal kMaximalWidth = qnGlobals->workbenchUnitSize() * kMaxCells;
            static const qreal kMaximalHeight = kMaximalWidth
                / qnGlobals->defaultLayoutCellAspectRatio();
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
        return expanded(m_aspectRatio, result, Qt::KeepAspectRatioByExpanding);

    return result;
}

QRectF QnResourceWidget::channelRect(int channel) const
{
    /* Channel rect is handled at shader level if dewarping is enabled. */
    QRectF rect = ((m_options & DisplayDewarped) || zoomRect().isNull()) ? this->rect() : unsubRect(this->rect(), zoomRect());

    if (m_channelsLayout->channelCount() == 1)
        return rect;

    QSizeF channelSize = cwiseDiv(rect.size(), m_channelsLayout->size());
    return QRectF(
        rect.topLeft() + cwiseMul(m_channelsLayout->position(channel), channelSize),
        channelSize
    );
}

// TODO: #Elric remove useRelativeCoordinates
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
        return QnGeometry::toSubRect(channelRect, result);
    }
    else
    {
        return result;
    }
}

void QnResourceWidget::registerButtonStatisticsAlias(QnImageButtonWidget* button, const QString& alias)
{
    context()->statisticsModule()->registerButton(alias, button);
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
    if (!m_options.testFlag(WindowRotationForbidden))
        result |= Qn::RotateButton;

    Qn::Permissions requiredPermissions = Qn::WritePermission | Qn::AddRemoveItemsPermission;
    if ((accessController()->permissions(item()->layout()->resource()) & requiredPermissions) == requiredPermissions)
        result |= Qn::CloseButton;

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

void QnResourceWidget::ensureAboutToBeDestroyedEmitted()
{
    if (m_aboutToBeDestroyedEmitted)
        return;

    m_aboutToBeDestroyedEmitted = true;
    emit aboutToBeDestroyed();
}

void QnResourceWidget::setOption(Option option, bool value /*= true*/)
{
    setOptions(value ? m_options | option : m_options & ~option);
}

void QnResourceWidget::setOptions(Options options)
{
    if (m_options == options)
        return;

    Options changedOptions = m_options ^ options;
    m_options = options;

    optionsChangedNotify(changedOptions);
    emit optionsChanged();
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

Qn::ResourceStatusOverlay QnResourceWidget::calculateStatusOverlay(int resourceStatus, bool hasVideo) const
{
    if (resourceStatus == Qn::Offline)
    {
        return Qn::OfflineOverlay;
    }
    else if (resourceStatus == Qn::Unauthorized)
    {
        return Qn::UnauthorizedOverlay;
    }
    else if (m_renderStatus == Qn::NewFrameRendered)
    {
        return Qn::EmptyOverlay;
    }
    else if (!hasVideo)
    {
        return Qn::NoVideoDataOverlay;
    }
    else if (m_renderStatus == Qn::NothingRendered || m_renderStatus == Qn::CannotRender)
    {
        return Qn::LoadingOverlay;
    }
    else if (QDateTime::currentMSecsSinceEpoch() - m_lastNewFrameTimeMSec >= defaultLoadingTimeoutMSec)
    {
        /* m_renderStatus is OldFrameRendered at this point. */
        return Qn::LoadingOverlay;
    }
    else
    {
        return Qn::EmptyOverlay;
    }
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

    bool isInactiveInFullScreen = (options().testFlag(FullScreenMode)
                                   && !options().testFlag(ActivityPresence));

    bool overlaysCanBeVisible = (!isInactiveInFullScreen
                                 && !options().testFlag(QnResourceWidget::InfoOverlaysForbidden));

    bool detailsVisible = m_options.testFlag(DisplayInfo);
    if (QnImageButtonWidget *infoButton = titleBar()->rightButtonsBar()->button(Qn::InfoButton))
        infoButton->setChecked(detailsVisible);

    bool alwaysShowName = m_options.testFlag(AlwaysShowName);

    const bool showOnlyCameraName = ((overlaysCanBeVisible && detailsVisible) || alwaysShowName)
        && !m_mouseInWidget;
    const bool showCameraNameWithButtons = overlaysCanBeVisible && m_mouseInWidget;
    const bool showPosition = overlaysCanBeVisible && (detailsVisible || m_mouseInWidget);
    const bool showDetailedInfo = overlaysCanBeVisible && detailsVisible && (m_mouseInWidget || qnRuntime->showFullInfo());

    const bool showButtonsOverlay = (showOnlyCameraName || showCameraNameWithButtons);

    const bool updatePositionTextRequired = (showPosition && !isOverlayWidgetVisible(m_hudOverlay->position()));
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

    /* Update overlay. */
    m_renderStatus = renderStatus;
    if (renderStatus == Qn::NewFrameRendered)
        m_lastNewFrameTimeMSec = QDateTime::currentMSecsSinceEpoch();
    updateStatusOverlay(animate);
    emit painted();
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
            const bool selected = (m_options.testFlag(DisplaySelection) && isSelected());
            const bool focused = isSelected();
            const bool active = isLocalActive();

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
    emit selectionStateChanged(m_selectionState);
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
    const auto visibleButtons = titleBar()->rightButtonsBar()->visibleButtons();
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
