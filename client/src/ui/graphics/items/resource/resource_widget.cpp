#include "resource_widget.h"

#include <cassert>
#include <cmath>

#include <QtGui/QPainter>
#include <QtWidgets/QGraphicsScene>
#include <QtWidgets/QGraphicsView>
#include <QtWidgets/QGraphicsLinearLayout>

#include <client/client_settings.h>
#include <client/client_runtime_settings.h>

#include <utils/common/warnings.h>
#include <utils/common/scoped_painter_rollback.h>
#include <utils/common/util.h>
#include <utils/common/synctime.h>
#include <utils/math/color_transformations.h>
#include <utils/math/linear_combination.h>

#include <core/resource/resource_media_layout.h>
#include <core/resource/security_cam_resource.h>
#include <core/resource/layout_resource.h>
#include <core/resource_management/resource_pool.h>

#include <ui/common/cursor_cache.h>
#include <ui/common/palette.h>
#include <ui/help/help_topics.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/animation/opacity_animator.h>
#include <ui/graphics/opengl/gl_shortcuts.h>
#include <ui/graphics/opengl/gl_context_data.h>
#include <ui/graphics/instruments/motion_selection_instrument.h>
#include <ui/graphics/items/resource/button_ids.h>
#include <ui/graphics/items/controls/html_text_item.h>
#include <ui/graphics/items/standard/graphics_label.h>
#include <ui/graphics/items/generic/proxy_label.h>
#include <ui/graphics/items/generic/image_button_widget.h>
#include <ui/graphics/items/generic/image_button_bar.h>
#include <ui/graphics/items/generic/viewport_bound_widget.h>
#include <ui/graphics/items/overlays/buttons_overlay.h>
#include <ui/graphics/items/overlays/resource_status_overlay_widget.h>
#include <ui/graphics/items/overlays/scrollable_overlay_widget.h>
#include <ui/workbench/workbench.h>
#include <ui/workbench/workbench_item.h>
#include <ui/workbench/workbench_layout.h>
#include <ui/workbench/workbench_display.h>
#include <ui/workbench/workbench_access_controller.h>
#include <ui/style/globals.h>
#include <ui/style/skin.h>
#include <utils/aspect_ratio.h>
#include <utils/license_usage_helper.h>
#include <utils/common/string.h>

namespace
{
    const auto kButtonsSize = 24.0;

    /** Frame extension multiplier determines the width of frame extension relative
     * to frame width.
     *
     * Frame events are processed not only when hovering over the frame itself,
     * but also over its extension. */
    const qreal frameExtensionMultiplier = 1.0;

    /** Default timeout before the video is displayed as "loading", in milliseconds. */
#ifdef QN_RESOURCE_WIDGET_FLASHY_LOADING_OVERLAY
    const qint64 defaultLoadingTimeoutMSec = MAX_FRAME_DURATION;
#else
    const qint64 defaultLoadingTimeoutMSec = MAX_FRAME_DURATION * 3;
#endif

    /** Background color for overlay panels. */

    const QColor infoBackgroundColor = QColor(0, 0, 0, 127); // TODO: #gdm #customization

    const QColor overlayTextColor = QColor(255, 255, 255); // TODO: #gdm #customization

    const float noAspectRatio = -1.0;

    //Q_GLOBAL_STATIC(QnDefaultResourceVideoLayout, qn_resourceWidget_defaultContentLayout);
    QSharedPointer<QnDefaultResourceVideoLayout> qn_resourceWidget_defaultContentLayout( new QnDefaultResourceVideoLayout() ); // TODO: #Elric get rid of this

    void splitFormat(const QString &format, QString *left, QString *right) {
        int index = format.indexOf(QLatin1Char('\t'));
        if(index != -1) {
            *left = format.mid(0, index);
            *right = format.mid(index + 1);
        } else {
            *left = format;
            *right = QString();
        }
    }

    bool itemBelongsToValidLayout(QnWorkbenchItem *item) {
        return (item
            && item->layout()
            && item->layout()->resource()
            && item->layout()->resource()->resourcePool()
            && !item->layout()->resource()->getParentId().isNull());
    }
} // anonymous namespace

struct QnResourceWidget::OverlayWidgets
{
    QnButtonsOverlay *buttonsOverlay;

    GraphicsWidget *detailsOverlay;     /**< Overlay containing info item. */
    QnHtmlTextItem *detailsItem;        /**< Detailed camera info (resolution, stream, etc). */

    GraphicsWidget *positionOverlay;    /**< Overlay containing position item. */
    QnHtmlTextItem *positionItem;       /**< Current camera position. */

    OverlayWidgets();
};

QnResourceWidget::OverlayWidgets::OverlayWidgets()
    : buttonsOverlay(nullptr)

    , detailsOverlay(nullptr)
    , detailsItem(new QnHtmlTextItem())

    , positionOverlay(nullptr)
    , positionItem(new QnHtmlTextItem())
{}

// -------------------------------------------------------------------------- //
// Logic
// -------------------------------------------------------------------------- //
QnResourceWidget::QnResourceWidget(QnWorkbenchContext *context, QnWorkbenchItem *item, QGraphicsItem *parent):
    base_type(parent),
    QnWorkbenchContextAware(context),
    m_item(item),
    m_options(DisplaySelection),
    m_localActive(false),
    m_frameOpacity(1.0),
    m_frameWidth(-1.0),
    m_titleTextFormat(lit("%1")),
    m_titleTextFormatHasPlaceholder(true),
    m_overlayWidgets(new OverlayWidgets()),
    m_aboutToBeDestroyedEmitted(false),
    m_mouseInWidget(false),
    m_statusOverlay(Qn::EmptyOverlay),
    m_renderStatus(Qn::NothingRendered),
    m_lastNewFrameTimeMSec(0)
{
    setAcceptHoverEvents(true);
    setTransformOrigin(Center);

    /* Initialize resource. */
    m_resource = qnResPool->getResourceByUniqueId(item->resourceUid());
    connect(m_resource, &QnResource::nameChanged, this, &QnResourceWidget::updateTitleText);

    /* Set up frame. */
    setFrameWidth(0.0);

    /* Set up overlay widgets. */
    QFont font = this->font();
    font.setStyleName(lit("Arial"));
    font.setPixelSize(15);
    setFont(font);

    setPaletteColor(this, QPalette::WindowText, overlayTextColor);

    addInfoOverlay();
    addMainOverlay();
    createButtons();
    /* Handle layout permissions if an item is placed on the common layout. Otherwise, it can be Motion Widget, for example. */
    if (itemBelongsToValidLayout(item))
        connect(accessController()->notifier(item->layout()->resource()), &QnWorkbenchPermissionsNotifier::permissionsChanged, this, &QnResourceWidget::updateButtonsVisibility);

    /* Status overlay. */
    m_statusOverlayWidget = new QnStatusOverlayWidget(m_resource, this);
    addOverlayWidget(m_statusOverlayWidget
        , detail::OverlayParams(UserVisible, true, false, StatusLayer));


    /* Initialize resource. */
    m_resource = qnResPool->getResourceByUniqueId(item->resourceUid());
    connect(m_resource, &QnResource::nameChanged, this, &QnResourceWidget::updateTitleText);
    setChannelLayout(qn_resourceWidget_defaultContentLayout);

    m_aspectRatio = defaultAspectRatio();

    connect(item, &QnWorkbenchItem::dataChanged, this, &QnResourceWidget::at_itemDataChanged);

    /* Videowall license changes helper */
    QnLicenseUsageWatcher* videowallLicenseHelper = new QnVideoWallLicenseUsageWatcher(this);
    connect(videowallLicenseHelper, &QnLicenseUsageWatcher::licenseUsageChanged, this, &QnResourceWidget::updateStatusOverlay);

    /* Run handlers. */
    setInfoVisible(buttonsOverlay()->rightButtonsBar()->button(Qn::InfoButton)->isChecked(), false);
    updateTitleText();
    updateButtonsVisibility();
    updateCursor();

    connect(this, &QnResourceWidget::rotationChanged, this, [this]() {
        if (m_enclosingGeometry.isValid())
            setGeometry(calculateGeometry(m_enclosingGeometry));
    });
}

QnResourceWidget::~QnResourceWidget() {
    ensureAboutToBeDestroyedEmitted();
}

//TODO: #ynikitenkov #high emplace back "titleLayout->setContentsMargins(0, 0, 0, 1);" fix
void QnResourceWidget::addInfoOverlay()
{
    {
        QnHtmlTextItemOptions infoOptions;
        infoOptions.backgroundColor = infoBackgroundColor;
        infoOptions.borderRadius = 2;
        infoOptions.autosize = true;

        enum { kMargin = 2 };

        m_overlayWidgets->detailsItem->setOptions(infoOptions);
        m_overlayWidgets->detailsItem->setProperty(Qn::NoBlockMotionSelection, true);
        auto detailsOverlay = new QnScrollableOverlayWidget(Qt::AlignLeft, this);
        detailsOverlay->setProperty(Qn::NoBlockMotionSelection, true);
        detailsOverlay->setContentsMargins(kMargin, 0, 0, kMargin);
        detailsOverlay->addItem(m_overlayWidgets->detailsItem);
        detailsOverlay->setMaxFillCoeff(QSizeF(0.3, 0.8));
        addOverlayWidget(detailsOverlay
            , detail::OverlayParams(UserVisible, true, true, InfoLayer));
        m_overlayWidgets->detailsOverlay = detailsOverlay;
        setOverlayWidgetVisible(m_overlayWidgets->detailsOverlay, false, false);


        m_overlayWidgets->positionItem->setOptions(infoOptions);
        m_overlayWidgets->positionItem->setProperty(Qn::NoBlockMotionSelection, true);
        auto positionOverlay = new QnScrollableOverlayWidget(Qt::AlignRight, this);
        positionOverlay->setProperty(Qn::NoBlockMotionSelection, true);
        positionOverlay->setContentsMargins(0, 0, kMargin, kMargin);
        positionOverlay->addItem(m_overlayWidgets->positionItem);
        positionOverlay->setMaxFillCoeff(QSizeF(0.7, 0.8));
        addOverlayWidget(positionOverlay
            , detail::OverlayParams(UserVisible, true, true, InfoLayer));
        m_overlayWidgets->positionOverlay = positionOverlay;
        setOverlayWidgetVisible(m_overlayWidgets->positionOverlay, false, false);
    }
}

//TODO: #ynikitenkov headerLayout->setContentsMargins(0, 0, 0, 1);
void QnResourceWidget::addMainOverlay()
{
    m_overlayWidgets->buttonsOverlay = new QnButtonsOverlay(this);

    m_overlayWidgets->buttonsOverlay = new QnButtonsOverlay(this);
    addOverlayWidget(m_overlayWidgets->buttonsOverlay
        , detail::OverlayParams(UserVisible, true, true, InfoLayer));
    setOverlayWidgetVisible(m_overlayWidgets->buttonsOverlay, false, false);
}

void QnResourceWidget::createButtons() {
    QnImageButtonWidget *closeButton = new QnImageButtonWidget(lit("res_widget_close"));
    closeButton->setIcon(qnSkin->icon("item/close.png"));
    closeButton->setProperty(Qn::NoBlockMotionSelection, true);
    closeButton->setToolTip(tr("Close"));
    connect(closeButton, &QnImageButtonWidget::clicked, this, &QnResourceWidget::close);

    QnImageButtonWidget *infoButton = new QnImageButtonWidget(lit("res_widget_info"));
    infoButton->setIcon(qnSkin->icon("item/info.png"));
    infoButton->setCheckable(true);
    infoButton->setChecked(item()->displayInfo());
    infoButton->setProperty(Qn::NoBlockMotionSelection, true);
    infoButton->setToolTip(tr("Information"));
    connect(infoButton, &QnImageButtonWidget::toggled, this, &QnResourceWidget::at_infoButton_toggled);

    QnImageButtonWidget *rotateButton = new QnImageButtonWidget(lit("res_widget_rotate"));
    rotateButton->setIcon(qnSkin->icon("item/rotate.png"));
    rotateButton->setProperty(Qn::NoBlockMotionSelection, true);
    rotateButton->setToolTip(tr("Rotate"));
    setHelpTopic(rotateButton, Qn::MainWindow_MediaItem_Rotate_Help);
    connect(rotateButton, &QnImageButtonWidget::pressed, this, &QnResourceWidget::rotationStartRequested);
    connect(rotateButton, &QnImageButtonWidget::released, this, &QnResourceWidget::rotationStopRequested);

    auto buttonsBar = buttonsOverlay()->rightButtonsBar();
    buttonsBar->setUniformButtonSize(QSizeF(kButtonsSize, kButtonsSize));
    buttonsBar->addButton(Qn::CloseButton, closeButton);
    buttonsBar->addButton(Qn::InfoButton, infoButton);
    buttonsBar->addButton(Qn::RotateButton, rotateButton);

    connect(buttonsBar, SIGNAL(checkedButtonsChanged()), this, SLOT(at_buttonBar_checkedButtonsChanged()));

    auto iconButton = new QnImageButtonWidget(QString()); // It is non-clickable icon only, we don't need statistics alias here
    iconButton->setParent(this);
    iconButton->setPreferredSize(kButtonsSize, kButtonsSize);
    iconButton->setVisible(false);
    buttonsOverlay()->leftButtonsBar()->addButton(Qn::RecordingStatusIconButton, iconButton);
}

const QnResourcePtr &QnResourceWidget::resource() const {
    return m_resource;
}

QnWorkbenchItem* QnResourceWidget::item() const {
    return m_item.data();
}

const QRectF &QnResourceWidget::zoomRect() const {
    return m_zoomRect;
}

void QnResourceWidget::setZoomRect(const QRectF &zoomRect) {
    if(qFuzzyEquals(m_zoomRect, zoomRect))
        return;

    m_zoomRect = zoomRect;

    emit zoomRectChanged();
}

QnResourceWidget *QnResourceWidget::zoomTargetWidget() const {
    return QnWorkbenchContextAware::display()->zoomTargetWidget(const_cast<QnResourceWidget *>(this));
}

void QnResourceWidget::setFrameWidth(qreal frameWidth) {
    if(qFuzzyCompare(m_frameWidth, frameWidth))
        return;

    prepareGeometryChange();

    m_frameWidth = frameWidth;
    qreal extendedFrameWidth = m_frameWidth * (1.0 + frameExtensionMultiplier);
    setWindowFrameMargins(extendedFrameWidth, extendedFrameWidth, extendedFrameWidth, extendedFrameWidth);
}

QColor QnResourceWidget::frameDistinctionColor() const {
    return m_frameDistinctionColor;
}

void QnResourceWidget::setFrameDistinctionColor(const QColor &frameColor) {
    if(m_frameDistinctionColor == frameColor)
        return;

    m_frameDistinctionColor = frameColor;

    emit frameDistinctionColorChanged();
}

const QnResourceWidgetFrameColors &QnResourceWidget::frameColors() const {
    return m_frameColors;
}

void QnResourceWidget::setFrameColors(const QnResourceWidgetFrameColors &frameColors) {
    m_frameColors = frameColors;
}

void QnResourceWidget::setAspectRatio(float aspectRatio) {
    if(qFuzzyCompare(m_aspectRatio, aspectRatio))
        return;

    m_aspectRatio = aspectRatio;
    updateGeometry(); /* Discard cached size hints. */

    emit aspectRatioChanged();
}

float QnResourceWidget::visualAspectRatio() const {
    if (!hasAspectRatio())
        return -1;

    return QnAspectRatio::isRotated90(rotation()) ? 1 / m_aspectRatio : m_aspectRatio;
}

float QnResourceWidget::defaultVisualAspectRatio() const {
    if (m_enclosingGeometry.isNull())
        return defaultAspectRatio();

    return m_enclosingGeometry.width() / m_enclosingGeometry.height();
}

float QnResourceWidget::visualChannelAspectRatio() const {
    if (!channelLayout())
        return visualAspectRatio();

    qreal layoutAspectRatio = QnGeometry::aspectRatio(channelLayout()->size());
    if (QnAspectRatio::isRotated90(rotation()))
        return visualAspectRatio() * layoutAspectRatio;
    else
        return visualAspectRatio() / layoutAspectRatio;
}

QRectF QnResourceWidget::enclosingGeometry() const {
    return m_enclosingGeometry;
}

void QnResourceWidget::setEnclosingGeometry(const QRectF &enclosingGeometry, bool updateGeometry) {
    m_enclosingGeometry = enclosingGeometry;
    if (updateGeometry)
        setGeometry(calculateGeometry(enclosingGeometry));
}

QRectF QnResourceWidget::calculateGeometry(const QRectF &enclosingGeometry, qreal rotation) const {
    if (!enclosingGeometry.isEmpty()) {
        /* Calculate bounds of the rotated item. */
        qreal aspectRatio = hasAspectRatio() ? m_aspectRatio : defaultVisualAspectRatio();
        return encloseRotatedGeometry(enclosingGeometry, aspectRatio, rotation);
    } else {
        return enclosingGeometry;
    }
}

QRectF QnResourceWidget::calculateGeometry(const QRectF &enclosingGeometry) const {
    return calculateGeometry(enclosingGeometry, this->rotation());
}

QString QnResourceWidget::titleText() const
{
    return m_overlayWidgets->buttonsOverlay->titleLabel()->text();
}

QString QnResourceWidget::titleTextFormat() const
{
    return m_titleTextFormat;
}

void QnResourceWidget::setTitleTextFormat(const QString &titleTextFormat)
{
    if(m_titleTextFormat == titleTextFormat)
        return;

    m_titleTextFormat = titleTextFormat;
    m_titleTextFormatHasPlaceholder = titleTextFormat.contains(QLatin1String("%1"));

    updateTitleText();
}

void QnResourceWidget::setTitleTextInternal(const QString &titleText) {
    QString leftText, rightText;

    splitFormat(titleText, &leftText, &rightText);

    m_overlayWidgets->buttonsOverlay->titleLabel()->setText(leftText);
    m_overlayWidgets->buttonsOverlay->extraInfoLabel()->setText(rightText);
}

QString QnResourceWidget::calculateTitleText() const {
    enum {
        kMaxNameLength = 30
    };

    return elideString(m_resource->getName(), kMaxNameLength);
}

void QnResourceWidget::updateTitleText() {
    setTitleTextInternal(m_titleTextFormatHasPlaceholder ? m_titleTextFormat.arg(calculateTitleText()) : m_titleTextFormat);
}

void QnResourceWidget::updateInfoText() {
    updateDetailsText();
    updatePositionText();
}

QString QnResourceWidget::calculateDetailsText() const {
    return QString();
}

void QnResourceWidget::updateDetailsText() {
    if (!isOverlayWidgetVisible(m_overlayWidgets->detailsOverlay))
        return;

    const QString text = calculateDetailsText();
    m_overlayWidgets->detailsItem->setHtml(text);
    m_overlayWidgets->detailsItem->setVisible(!text.isEmpty());
}

QString QnResourceWidget::calculatePositionText() const {
    return QString();
}

void QnResourceWidget::updatePositionText() {
    if (!isOverlayWidgetVisible(m_overlayWidgets->positionOverlay))
        return;

    const QString text = calculatePositionText();
    m_overlayWidgets->positionItem->setHtml(text);
    m_overlayWidgets->positionItem->setVisible(!text.isEmpty());
}

QCursor QnResourceWidget::calculateCursor() const {
    return Qt::ArrowCursor;
}

void QnResourceWidget::updateCursor() {
    QCursor newCursor = calculateCursor();
    QCursor oldCursor = this->cursor();
    if(newCursor.shape() != oldCursor.shape() || newCursor.shape() == Qt::BitmapCursor)
        setCursor(newCursor);
}

QSizeF QnResourceWidget::constrainedSize(const QSizeF constraint, Qt::WindowFrameSection pinSection) const {
    if (!hasAspectRatio())
        return constraint;

    QSizeF result = constraint;

    switch (pinSection) {
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

void QnResourceWidget::updateCheckedButtons() {
    if (!item())
        return;

    const auto checkedButtons = item()->data(Qn::ItemCheckedButtonsRole).toInt();
    buttonsOverlay()->rightButtonsBar()->setCheckedButtons(checkedButtons);
}

QSizeF QnResourceWidget::sizeHint(Qt::SizeHint which, const QSizeF &constraint) const {
    QSizeF result = base_type::sizeHint(which, constraint);

    if(!hasAspectRatio())
        return result;

    if(which == Qt::MinimumSize)
        return expanded(m_aspectRatio, result, Qt::KeepAspectRatioByExpanding);

    return result;
}

QRectF QnResourceWidget::channelRect(int channel) const {
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
QRectF QnResourceWidget::exposedRect(int channel, bool accountForViewport, bool accountForVisibility, bool useRelativeCoordinates) {
    if(accountForVisibility && (!isVisible() || qFuzzyIsNull(effectiveOpacity())))
        return QRectF();

    QRectF channelRect = this->channelRect(channel);
    if (channelRect.isEmpty())
        return QRectF();

    QRectF result = channelRect.intersected(rect());
    if(result.isEmpty())
        return QRectF();

    if(accountForViewport) {
        if(scene()->views().empty())
            return QRectF();
        QGraphicsView *view = scene()->views()[0];

        QRectF viewportRect = mapRectFromScene(QnSceneTransformations::mapRectToScene(view, view->viewport()->rect()));
        result = result.intersected(viewportRect);
        if(result.isEmpty())
            return QRectF();
    }

    if(useRelativeCoordinates) {
        return QnGeometry::toSubRect(channelRect, result);
    } else {
        return result;
    }
}

Qn::RenderStatus QnResourceWidget::renderStatus() const {
    return m_renderStatus;
}

bool QnResourceWidget::isLocalActive() const {
    return m_localActive;
}

void QnResourceWidget::setLocalActive(bool localActive) {
    m_localActive = localActive;
}

QnButtonsOverlay *QnResourceWidget::buttonsOverlay() const
{
    return m_overlayWidgets->buttonsOverlay;
}


void QnResourceWidget::setCheckedButtons(int buttons)
{
    buttonsOverlay()->rightButtonsBar()->setCheckedButtons(buttons);
    buttonsOverlay()->leftButtonsBar()->setCheckedButtons(buttons);
}

int QnResourceWidget::checkedButtons() const
{
    return (buttonsOverlay()->rightButtonsBar()->checkedButtons()
        | buttonsOverlay()->leftButtonsBar()->checkedButtons());
}

int QnResourceWidget::visibleButtons() const
{
    return (buttonsOverlay()->rightButtonsBar()->visibleButtons()
        | buttonsOverlay()->leftButtonsBar()->visibleButtons());
}

int QnResourceWidget::calculateButtonsVisibility() const {
    int result = Qn::InfoButton;

    if (!(m_options & WindowRotationForbidden))
        result |= Qn::RotateButton;

    Qn::Permissions requiredPermissions = Qn::WritePermission | Qn::AddRemoveItemsPermission;
    if((accessController()->permissions(item()->layout()->resource()) & requiredPermissions) == requiredPermissions)
        result |= Qn::CloseButton;

    return result;
}

void QnResourceWidget::updateButtonsVisibility() {
    // TODO: #ynikitenkov Change destroying sequence: items should be destroyed before layout
    if (!item() || !item()->layout())
        return;

    const auto visibleButtons = calculateButtonsVisibility()
        & ~(item()->data<int>(Qn::ItemDisabledButtonsRole, 0));

    buttonsOverlay()->rightButtonsBar()->setVisibleButtons(visibleButtons);
    buttonsOverlay()->leftButtonsBar()->setVisibleButtons(visibleButtons);
}

QCursor QnResourceWidget::windowCursorAt(Qn::WindowFrameSection section) const {
    if(section == Qn::NoSection)
        return calculateCursor();

    return base_type::windowCursorAt(section);
}

int QnResourceWidget::helpTopicAt(const QPointF &) const {
    return -1;
}

void QnResourceWidget::ensureAboutToBeDestroyedEmitted() {
    if(m_aboutToBeDestroyedEmitted)
        return;

    m_aboutToBeDestroyedEmitted = true;
    emit aboutToBeDestroyed();
}

void QnResourceWidget::setOptions(Options options) {
    if(m_options == options)
        return;

    Options changedOptions = m_options ^ options;
    m_options = options;

    optionsChangedNotify(changedOptions);
    emit optionsChanged();
}

const QSize &QnResourceWidget::channelScreenSize() const {
    return m_channelScreenSize;
}

void QnResourceWidget::setChannelScreenSize(const QSize &size) {
    if(size == m_channelScreenSize)
        return;

    m_channelScreenSize = size;

    channelScreenSizeChangedNotify();
}

bool QnResourceWidget::isInfoVisible() const {
    return options().testFlag(DisplayInfo);
}

void QnResourceWidget::setInfoVisible(bool visible, bool animate) {
    if (isInfoVisible() == visible)
        return;

    setOption(DisplayInfo, visible);
    item()->setDisplayInfo(visible);
    updateHud(animate);
}

Qn::ResourceStatusOverlay QnResourceWidget::statusOverlay() const {
    return m_statusOverlay;
}

void QnResourceWidget::setStatusOverlay(Qn::ResourceStatusOverlay statusOverlay, bool animate)
{
    if(m_statusOverlay == statusOverlay)
        return;

    m_statusOverlay = statusOverlay;
    m_statusOverlayWidget->setStatusOverlay(statusOverlay);

    qreal opacity = statusOverlay == Qn::EmptyOverlay
        ? 0.0
        : 1.0;

    if(animate)
        opacityAnimator(m_statusOverlayWidget)->animateTo(opacity);
    else
        m_statusOverlayWidget->setOpacity(opacity);
}

Qn::ResourceStatusOverlay QnResourceWidget::calculateStatusOverlay(int resourceStatus, bool hasVideo) const {
    if (resourceStatus == Qn::Offline) {
        return Qn::OfflineOverlay;
    } else if (resourceStatus == Qn::Unauthorized) {
        return Qn::UnauthorizedOverlay;
    } else if(m_renderStatus == Qn::NewFrameRendered) {
        return Qn::EmptyOverlay;
    } else if(!hasVideo) {
        return Qn::NoVideoDataOverlay;
    } else if(m_renderStatus == Qn::NothingRendered || m_renderStatus == Qn::CannotRender) {
        return Qn::LoadingOverlay;
    } else if(QDateTime::currentMSecsSinceEpoch() - m_lastNewFrameTimeMSec >= defaultLoadingTimeoutMSec) {
        /* m_renderStatus is OldFrameRendered at this point. */
        return Qn::LoadingOverlay;
    } else {
        return Qn::EmptyOverlay;
    }
}

Qn::ResourceStatusOverlay QnResourceWidget::calculateStatusOverlay() const {
    const auto mediaRes = m_resource.dynamicCast<QnMediaResource>();
    return calculateStatusOverlay(m_resource->getStatus(), mediaRes && mediaRes->hasVideo(0));
}

void QnResourceWidget::updateStatusOverlay() {
    setStatusOverlay(calculateStatusOverlay());
}

void QnResourceWidget::setChannelLayout(QnConstResourceVideoLayoutPtr channelLayout) {
    if(m_channelsLayout == channelLayout)
        return;

    m_channelsLayout = channelLayout;

    channelLayoutChangedNotify();
}

int QnResourceWidget::channelCount() const {
    return m_channelsLayout->channelCount();
}

void QnResourceWidget::updateHud(bool animate) {

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
    if(QnImageButtonWidget *infoButton = buttonsOverlay()->rightButtonsBar()->button(Qn::InfoButton))
        infoButton->setChecked(detailsVisible);

    bool alwaysShowName = m_options.testFlag(AlwaysShowName);

    const bool showOnlyCameraName         = ((overlaysCanBeVisible && detailsVisible) || alwaysShowName) && !m_mouseInWidget;
    const bool showCameraNameWithButtons  = overlaysCanBeVisible && m_mouseInWidget;
    const bool showPosition               = overlaysCanBeVisible && (detailsVisible || m_mouseInWidget);
    const bool showDetailedInfo           = overlaysCanBeVisible && detailsVisible && (m_mouseInWidget || qnRuntime->showFullInfo());

    const bool showButtonsOverlay = (showOnlyCameraName || showCameraNameWithButtons);


    bool updatePositionTextRequired = (showPosition && !isOverlayWidgetVisible(m_overlayWidgets->positionOverlay));
    setOverlayWidgetVisible(m_overlayWidgets->positionOverlay,               showPosition,               animate);
    if (updatePositionTextRequired)
        updatePositionText();

    bool updateDetailsTextRequired = (showDetailedInfo && !isOverlayWidgetVisible(m_overlayWidgets->detailsOverlay));
    setOverlayWidgetVisible(m_overlayWidgets->detailsOverlay,                showDetailedInfo,           animate);
    if (updateDetailsTextRequired)
        updateDetailsText();

    setOverlayWidgetVisible(m_overlayWidgets->buttonsOverlay, showButtonsOverlay, animate);
    m_overlayWidgets->buttonsOverlay->setSimpleMode(showOnlyCameraName);
}

bool QnResourceWidget::isHovered() const {
    return m_mouseInWidget;
}

// -------------------------------------------------------------------------- //
// Painting
// -------------------------------------------------------------------------- //
void QnResourceWidget::paint(QPainter *painter, const QStyleOptionGraphicsItem * /*option*/, QWidget * /*widget*/) {
    QnScopedPainterPenRollback penRollback(painter);
    QnScopedPainterBrushRollback brushRollback(painter);
    QnScopedPainterFontRollback fontRollback(painter);

    /* Update screen size of a single channel. */
    setChannelScreenSize(painter->combinedTransform().mapRect(channelRect(0)).size().toSize());

    Qn::RenderStatus renderStatus = Qn::NothingRendered;

    for(int i = 0; i < channelCount(); i++) {
        /* Draw content. */
        QRectF channelRect = this->channelRect(i);
        QRectF paintRect = this->exposedRect(i, false, false, false);
        if(paintRect.isEmpty())
            continue;

        renderStatus = qMax(renderStatus, paintChannelBackground(painter, i, channelRect, paintRect));

        /* Draw foreground. */
        paintChannelForeground(painter, i, paintRect);

        /* Draw selected / not selected overlay. */
        paintSelection(painter, paintRect);
    }

    /* Update overlay. */
    m_renderStatus = renderStatus;
    if(renderStatus == Qn::NewFrameRendered)
        m_lastNewFrameTimeMSec = QDateTime::currentMSecsSinceEpoch();
    updateStatusOverlay();

    emit painted();
}

void QnResourceWidget::paintChannelForeground(QPainter *, int, const QRectF &) {
    return;
}

void QnResourceWidget::paintWindowFrame(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *) {
    if(qFuzzyIsNull(m_frameOpacity))
        return;

    QColor color;
    if(isSelected()) {
        color = m_frameColors.selected;
    } else if(isLocalActive()) {
        if(m_frameDistinctionColor.isValid()) {
            color = m_frameDistinctionColor.lighter();
        } else {
            color = m_frameColors.active;
        }
    } else {
        if(m_frameDistinctionColor.isValid()) {
            color = m_frameDistinctionColor;
        } else {
            color = m_frameColors.normal;
        }
    }

    QSizeF size = this->size();
    qreal w = size.width();
    qreal h = size.height();
    qreal fw = m_frameWidth;

    QnScopedPainterOpacityRollback opacityRollback(painter, painter->opacity() * m_frameOpacity);
    QnScopedPainterAntialiasingRollback antialiasingRollback(painter, true); /* Antialiasing is here for a reason. Without it border looks crappy. */
    painter->fillRect(QRectF(-fw,     -fw,     w + fw * 2,  fw), color);
    painter->fillRect(QRectF(-fw,     h,       w + fw * 2,  fw), color);
    painter->fillRect(QRectF(-fw,     0,       fw,          h),  color);
    painter->fillRect(QRectF(w,       0,       fw,          h),  color);
}

void QnResourceWidget::paintSelection(QPainter *painter, const QRectF &rect) {
    if(!isSelected())
        return;

    if(!(m_options & DisplaySelection))
        return;

    painter->fillRect(rect, palette().color(QPalette::Highlight));
}

float QnResourceWidget::defaultAspectRatio() const {
    if (item())
        return item()->data(Qn::ItemAspectRatioRole, noAspectRatio);
    return noAspectRatio;
}


// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //
bool QnResourceWidget::windowFrameEvent(QEvent *event) {
    bool result = base_type::windowFrameEvent(event);

    if(event->type() == QEvent::GraphicsSceneHoverMove) {
        QGraphicsSceneHoverEvent *e = static_cast<QGraphicsSceneHoverEvent *>(event);

        /* Qt does not unset a cursor unless mouse pointer leaves widget's frame.
         *
         * As this widget may not have a frame section associated with some parts of
         * its frame, cursor must be unset manually. */
        Qt::WindowFrameSection section = windowFrameSectionAt(e->pos());
        if(section == Qt::NoSection)
            updateCursor();
    }

    return result;
}

void QnResourceWidget::hoverEnterEvent(QGraphicsSceneHoverEvent *event) {
    m_mouseInWidget = true;

    setOverlayVisible();
    updateHud();
    base_type::hoverEnterEvent(event);
}

void QnResourceWidget::hoverMoveEvent(QGraphicsSceneHoverEvent *event) {
    setOverlayVisible();
    base_type::hoverMoveEvent(event);
}

void QnResourceWidget::hoverLeaveEvent(QGraphicsSceneHoverEvent *event) {
    m_mouseInWidget = false;

    setOverlayVisible(false);
    updateHud();
    base_type::hoverLeaveEvent(event);
}

void QnResourceWidget::optionsChangedNotify(Options changedFlags)
{
    const auto visibleButtons = buttonsOverlay()->rightButtonsBar()->visibleButtons();
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
        updateHud(true);
}

void QnResourceWidget::at_itemDataChanged(int role) {
    if (role != Qn::ItemCheckedButtonsRole)
        return;
    updateCheckedButtons();
}

void QnResourceWidget::at_infoButton_toggled(bool toggled) {
    setInfoVisible(toggled);
}

void QnResourceWidget::at_buttonBar_checkedButtonsChanged() {
    if (!item())
        return;

    const auto checkedButtons = buttonsOverlay()->rightButtonsBar()->checkedButtons();
    item()->setData(Qn::ItemCheckedButtonsRole, checkedButtons);
    update();
}

