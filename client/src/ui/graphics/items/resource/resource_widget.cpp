#include "resource_widget.h"

#include <cassert>
#include <cmath>

#include <QtGui/QPainter>
#include <QtWidgets/QGraphicsScene>
#include <QtWidgets/QGraphicsView>
#include <QtWidgets/QGraphicsLinearLayout>

#include <client/client_settings.h>

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
#include <ui/graphics/items/standard/graphics_label.h>
#include <ui/graphics/items/generic/image_button_widget.h>
#include <ui/graphics/items/generic/image_button_bar.h>
#include <ui/graphics/items/generic/viewport_bound_widget.h>
#include <ui/graphics/items/overlays/resource_status_overlay_widget.h>
#include <ui/workbench/workbench_item.h>
#include <ui/workbench/workbench_layout.h>
#include <ui/workbench/workbench_display.h>
#include <ui/workbench/workbench_access_controller.h>
#include <ui/style/globals.h>
#include <ui/style/skin.h>

#include <utils/license_usage_helper.h>

namespace {

    /** Frame extension multiplier determines the width of frame extension relative
     * to frame width.
     *
     * Frame events are processed not only when hovering over the frame itself,
     * but also over its extension. */
    const qreal frameExtensionMultiplier = 1.0;

    /** Default shadow displacement, in scene coordinates. */
    const QPointF defaultShadowDisplacement = QPointF(qnGlobals->workbenchUnitSize(), qnGlobals->workbenchUnitSize()) * 0.05;

    /** Default timeout before the video is displayed as "loading", in milliseconds. */
#ifdef QN_RESOURCE_WIDGET_FLASHY_LOADING_OVERLAY
    const qint64 defaultLoadingTimeoutMSec = MAX_FRAME_DURATION;
#else
    const qint64 defaultLoadingTimeoutMSec = MAX_FRAME_DURATION * 3;
#endif

    /** Background color for overlay panels. */
    const QColor overlayBackgroundColor = QColor(0, 0, 0, 96); // TODO: #Elric #customization

    const QColor overlayTextColor = QColor(255, 255, 255, 160); // TODO: #Elric #customization

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

    QString mergeFormat(const QString &left, const QString &right) {
        return right.isEmpty() ? left : (left + QLatin1Char('\t') + right);
    }

} // anonymous namespace


// -------------------------------------------------------------------------- //
// Logic
// -------------------------------------------------------------------------- //
QnResourceWidget::QnResourceWidget(QnWorkbenchContext *context, QnWorkbenchItem *item, QGraphicsItem *parent):
    base_type(parent),
    QnWorkbenchContextAware(context),
    m_item(item),
    m_options(DisplaySelection | DisplayButtons),
    m_localActive(false),
    m_frameOpacity(1.0),
    m_frameWidth(-1.0),
    m_titleTextFormat(lit("%1")),
    m_infoTextFormat(lit("%1")),
    m_titleTextFormatHasPlaceholder(true),
    m_infoTextFormatHasPlaceholder(true),
    m_aboutToBeDestroyedEmitted(false),
    m_mouseInWidget(false),
    m_statusOverlay(Qn::EmptyOverlay),
    m_renderStatus(Qn::NothingRendered),
    m_lastNewFrameTimeMSec(0)
{
    setAcceptHoverEvents(true);
    setTransformOrigin(Center);

    /* Set up shadow. */
    setShadowDisplacement(defaultShadowDisplacement);

    /* Set up frame. */
    setFrameWidth(0.0);

    /* Set up overlay widgets. */
    QFont font = this->font();
    font.setPixelSize(20); 
    setFont(font);
    setPaletteColor(this, QPalette::WindowText, overlayTextColor);

    /* Header overlay. */
    m_headerLeftLabel = new GraphicsLabel();
    m_headerLeftLabel->setAcceptedMouseButtons(0);
    m_headerLeftLabel->setPerformanceHint(GraphicsLabel::PixmapCaching);

    m_headerRightLabel = new GraphicsLabel();
    m_headerRightLabel->setAcceptedMouseButtons(0);
    m_headerRightLabel->setPerformanceHint(GraphicsLabel::PixmapCaching);

    QnImageButtonWidget *closeButton = new QnImageButtonWidget();
    closeButton->setIcon(qnSkin->icon("item/close.png"));
    closeButton->setProperty(Qn::NoBlockMotionSelection, true);
    closeButton->setToolTip(tr("Close"));
    connect(closeButton, &QnImageButtonWidget::clicked, this, &QnResourceWidget::close);
    connect(accessController()->notifier(item->layout()->resource()), &QnWorkbenchPermissionsNotifier::permissionsChanged, this, &QnResourceWidget::updateButtonsVisibility);

    QnImageButtonWidget *infoButton = new QnImageButtonWidget();
    infoButton->setIcon(qnSkin->icon("item/info.png"));
    infoButton->setCheckable(true);
    infoButton->setProperty(Qn::NoBlockMotionSelection, true);
    infoButton->setToolTip(tr("Information"));
    connect(infoButton, &QnImageButtonWidget::toggled, this, &QnResourceWidget::at_infoButton_toggled);
    
    QnImageButtonWidget *rotateButton = new QnImageButtonWidget();
    rotateButton->setIcon(qnSkin->icon("item/rotate.png"));
    rotateButton->setProperty(Qn::NoBlockMotionSelection, true);
    rotateButton->setToolTip(tr("Rotate"));
    setHelpTopic(rotateButton, Qn::MainWindow_MediaItem_Rotate_Help);
    connect(rotateButton, &QnImageButtonWidget::pressed, this, &QnResourceWidget::rotationStartRequested);
    connect(rotateButton, &QnImageButtonWidget::released, this, &QnResourceWidget::rotationStopRequested);

    m_buttonBar = new QnImageButtonBar();
    m_buttonBar->setUniformButtonSize(QSizeF(24.0, 24.0));
    m_buttonBar->addButton(CloseButton, closeButton);
    m_buttonBar->addButton(InfoButton, infoButton);
    m_buttonBar->addButton(RotateButton, rotateButton);
    connect(m_buttonBar, SIGNAL(checkedButtonsChanged()), this, SLOT(at_buttonBar_checkedButtonsChanged()));

    m_iconButton = new QnImageButtonWidget();
    m_iconButton->setParent(this);
    m_iconButton->setPreferredSize(24.0, 24.0);
    m_iconButton->setVisible(false);
    connect(m_iconButton, &QnImageButtonWidget::visibleChanged, this, &QnResourceWidget::at_iconButton_visibleChanged);

    m_headerLayout = new QGraphicsLinearLayout(Qt::Horizontal);
    m_headerLayout->setContentsMargins(0.0, 0.0, 0.0, 0.0);
    m_headerLayout->setSpacing(2.0);
    m_headerLayout->addItem(m_headerLeftLabel);
    m_headerLayout->addStretch(0x1000); /* Set large enough stretch for the buttons to be placed at the right end of the layout. */
    m_headerLayout->addItem(m_headerRightLabel);
    m_headerLayout->addItem(m_buttonBar);

    m_headerWidget = new GraphicsWidget();
    m_headerWidget->setLayout(m_headerLayout);
    m_headerWidget->setAcceptedMouseButtons(0);
    m_headerWidget->setAutoFillBackground(true);
    setPaletteColor(m_headerWidget, QPalette::Window, overlayBackgroundColor);

    QGraphicsLinearLayout *headerOverlayLayout = new QGraphicsLinearLayout(Qt::Vertical);
    headerOverlayLayout->setContentsMargins(0.0, 0.0, 0.0, 0.0);
    headerOverlayLayout->addItem(m_headerWidget);
    headerOverlayLayout->addStretch(0x1000);

    m_headerOverlayWidget = new QnViewportBoundWidget(this);
    m_headerOverlayWidget->setLayout(headerOverlayLayout);
    m_headerOverlayWidget->setAcceptedMouseButtons(0);
    m_headerOverlayWidget->setOpacity(0.0);
    addOverlayWidget(m_headerOverlayWidget, AutoVisible, true, true, true, true);


    /* Footer overlay. */
    m_footerLeftLabel = new GraphicsLabel();
    m_footerLeftLabel->setAcceptedMouseButtons(0);
    m_footerLeftLabel->setPerformanceHint(GraphicsLabel::PixmapCaching);

    m_footerRightLabel = new GraphicsLabel();
    m_footerRightLabel->setAcceptedMouseButtons(0);
    m_footerRightLabel->setPerformanceHint(GraphicsLabel::PixmapCaching);

    QGraphicsLinearLayout *footerLayout = new QGraphicsLinearLayout(Qt::Horizontal);
    footerLayout->setContentsMargins(0.0, 0.0, 0.0, 0.0);
    footerLayout->addItem(m_footerLeftLabel);
    footerLayout->addStretch(0x1000);
    footerLayout->addItem(m_footerRightLabel);

    m_footerWidget = new GraphicsWidget();
    m_footerWidget->setLayout(footerLayout);
    m_footerWidget->setAcceptedMouseButtons(0);
    m_footerWidget->setAutoFillBackground(true);
    setPaletteColor(m_footerWidget, QPalette::Window, overlayBackgroundColor);
    m_footerWidget->setOpacity(0.0);

    QGraphicsLinearLayout *footerOverlayLayout = new QGraphicsLinearLayout(Qt::Vertical);
    footerOverlayLayout->setContentsMargins(0.0, 0.0, 0.0, 0.0);
    footerOverlayLayout->addStretch(0x1000);
    footerOverlayLayout->addItem(m_footerWidget);

    m_footerOverlayWidget = new QnViewportBoundWidget(this);
    m_footerOverlayWidget->setLayout(footerOverlayLayout);
    m_footerOverlayWidget->setAcceptedMouseButtons(0);
    m_footerOverlayWidget->setOpacity(0.0);
    addOverlayWidget(m_footerOverlayWidget, AutoVisible, true, true, true);


    /* Status overlay. */
    m_statusOverlayWidget = new QnStatusOverlayWidget(this);
    addOverlayWidget(m_statusOverlayWidget, UserVisible, true);


    /* Initialize resource. */
    m_resource = qnResPool->getResourceByUniqId(item->resourceUid());
    connect(m_resource, &QnResource::nameChanged, this, &QnResourceWidget::updateTitleText);
    setChannelLayout(qn_resourceWidget_defaultContentLayout);

    m_aspectRatio = defaultAspectRatio();

    connect(item, &QnWorkbenchItem::dataChanged, this, &QnResourceWidget::at_itemDataChanged);

    /* Videowall license changes helper */
    QnLicenseUsageHelper* videowallLicenseHelper = new QnVideoWallLicenseUsageHelper(this);
    connect(videowallLicenseHelper, &QnLicenseUsageHelper::licensesChanged, this, &QnResourceWidget::updateStatusOverlay);

    /* Run handlers. */
    updateTitleText();
    updateButtonsVisibility();
    updateCursor();

    // calling after all nested constructors are finished
    QTimer::singleShot(1, this, SLOT(updateCheckedButtons()));

    connect(this, &QnResourceWidget::rotationChanged, this, [this]() {
        if (m_enclosingGeometry.isValid())
            setGeometry(calculateGeometry(m_enclosingGeometry));
    });
}

QnResourceWidget::~QnResourceWidget() {
    ensureAboutToBeDestroyedEmitted();
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

    invalidateShadowShape();
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

    QRectF enclosingGeometry = this->enclosingGeometry();
    m_aspectRatio = aspectRatio;

    updateGeometry(); /* Discard cached size hints. */
    setEnclosingGeometry(enclosingGeometry);

    emit aspectRatioChanged();
}

QRectF QnResourceWidget::enclosingGeometry() const {
    QRectF rect = m_enclosingGeometry;
    rect.moveCenter(geometry().center());
    return rect;
}

void QnResourceWidget::setEnclosingGeometry(const QRectF &enclosingGeometry) {
    m_enclosingGeometry = enclosingGeometry;
    setGeometry(calculateGeometry(enclosingGeometry));
}

QRectF QnResourceWidget::calculateGeometry(const QRectF &enclosingGeometry) const {
    if (!enclosingGeometry.isEmpty()) {
        /* Calculate bounds of the rotated item. */

        qreal aspectRatio = hasAspectRatio() ? m_aspectRatio : enclosingGeometry.width() / enclosingGeometry.height();

        /* 1. Take a rectangle with our aspect ratio */
        QRectF geom = expanded(aspectRatio, enclosingGeometry, Qt::KeepAspectRatio);

        /* 2. Rotate it */
        QPointF c = geom.center();
        QTransform transform;
        transform.translate(c.x(), c.y());
        transform.rotate(rotation());
        transform.translate(-c.x(), -c.y());
        QRectF rotated = transform.mapRect(geom);

        /* 3. Scale it to fit enclosing geometry */
        QSizeF scaledSize = bounded(expanded(rotated.size(), enclosingGeometry.size(), Qt::KeepAspectRatio), enclosingGeometry.size(), Qt::KeepAspectRatio);

        /* 4. Get scale factor */
        qreal scale = QnGeometry::scaleFactor(rotated.size(), scaledSize, Qt::KeepAspectRatio);

        /* 5. Scale original non-rotated geometry */
        qreal xdiff = geom.width() / 2.0 * (1.0 - scale);
        qreal ydiff = geom.height() / 2.0 * (1.0 - scale);
        geom.adjust(xdiff, ydiff, -xdiff, -ydiff);

        return geom;
    } else {
        return enclosingGeometry;
    }
}

QString QnResourceWidget::titleText() const {
    return m_headerLeftLabel->text();
}

QString QnResourceWidget::titleTextFormat() const {
    return m_titleTextFormat;
}

void QnResourceWidget::setTitleTextFormat(const QString &titleTextFormat) {
    if(m_titleTextFormat == titleTextFormat)
        return;

    m_titleTextFormat = titleTextFormat;
    m_titleTextFormatHasPlaceholder = titleTextFormat.contains(QLatin1String("%1"));

    updateTitleText();
}

void QnResourceWidget::setTitleTextInternal(const QString &titleText) {
    QString leftText, rightText;

    splitFormat(titleText, &leftText, &rightText);

    m_headerLeftLabel->setText(leftText);
    m_headerRightLabel->setText(rightText);
}

QString QnResourceWidget::calculateTitleText() const {
    return m_resource->getName();
}

void QnResourceWidget::updateTitleText() {
    setTitleTextInternal(m_titleTextFormatHasPlaceholder ? m_titleTextFormat.arg(calculateTitleText()) : m_titleTextFormat);
}

QString QnResourceWidget::infoText() {
    return mergeFormat(m_footerLeftLabel->text(), m_footerRightLabel->text());
}

QString QnResourceWidget::infoTextFormat() const {
    return m_infoTextFormat;
}

void QnResourceWidget::setInfoTextFormat(const QString &infoTextFormat) {
    if(m_infoTextFormat == infoTextFormat)
        return;

    m_infoTextFormat = infoTextFormat;
    m_infoTextFormatHasPlaceholder = infoTextFormat.contains(QLatin1String("%1"));

    updateInfoText();
}

void QnResourceWidget::setInfoTextInternal(const QString &infoText) {
    QString leftText, rightText;
    
    splitFormat(infoText, &leftText, &rightText);

    m_footerLeftLabel->setText(leftText);
    m_footerRightLabel->setText(rightText);
}

QString QnResourceWidget::calculateInfoText() const {
    return QString();
}

void QnResourceWidget::updateInfoText() {
    setInfoTextInternal(m_infoTextFormatHasPlaceholder ? m_infoTextFormat.arg(calculateInfoText()) : m_infoTextFormat);
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

QSizeF QnResourceWidget::constrainedSize(const QSizeF constraint) const {
    if(!hasAspectRatio())
        return constraint;

    return expanded(m_aspectRatio, constraint, Qt::KeepAspectRatioByExpanding);
}

void QnResourceWidget::updateCheckedButtons() {
    if (!item())
        return;

    setCheckedButtons(static_cast<Buttons>(item()->data(Qn::ItemCheckedButtonsRole).toInt()));
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

QnResourceWidget::Buttons QnResourceWidget::checkedButtons() const {
    return static_cast<Buttons>(buttonBar()->checkedButtons());
}

void QnResourceWidget::setCheckedButtons(Buttons checkedButtons) {
    buttonBar()->setCheckedButtons(checkedButtons);
}

QnResourceWidget::Buttons QnResourceWidget::visibleButtons() const {
    return static_cast<Buttons>(buttonBar()->visibleButtons());
}

QnResourceWidget::Buttons QnResourceWidget::calculateButtonsVisibility() const {
    Buttons result = InfoButton;

    if (!(m_options & WindowRotationForbidden))
        result |= RotateButton;

    if(item() && item()->layout()) {
        Qn::Permissions requiredPermissions = Qn::WritePermission | Qn::AddRemoveItemsPermission;
        if((accessController()->permissions(item()->layout()->resource()) & requiredPermissions) == requiredPermissions)
            result |= CloseButton;
    }

    return result;
}

void QnResourceWidget::updateButtonsVisibility() {
    m_buttonBar->setVisibleButtons(
        calculateButtonsVisibility() & 
        ~(item() ? item()->data<int>(Qn::ItemDisabledButtonsRole, 0): 0)
    );
}

Qn::WindowFrameSections QnResourceWidget::windowFrameSectionsAt(const QRectF &region) const {
    Qn::WindowFrameSections result = base_type::windowFrameSectionsAt(region);

    /* This widget has no side frame sections if aspect ratio is set. */
    if(hasAspectRatio())
        result &= ~Qn::SideSections;

    return result;
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

    if(changedOptions & DisplayButtons)
        m_headerOverlayWidget->setVisible(options & DisplayButtons);

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
    updateInfoVisiblity(animate);

    setOverlayVisible(visible || m_mouseInWidget, animate);
}

Qn::ResourceStatusOverlay QnResourceWidget::statusOverlay() const {
    return m_statusOverlay;
}

void QnResourceWidget::setStatusOverlay(Qn::ResourceStatusOverlay statusOverlay) {
    if(m_statusOverlay == statusOverlay)
        return;

    m_statusOverlay = statusOverlay;

    if(statusOverlay == Qn::EmptyOverlay) {
        opacityAnimator(m_statusOverlayWidget)->animateTo(0.0);
    } else {
        opacityAnimator(m_statusOverlayWidget)->animateTo(1.0);
        m_statusOverlayWidget->setStatusOverlay(statusOverlay);
    }
}

Qn::ResourceStatusOverlay QnResourceWidget::calculateStatusOverlay(int resourceStatus) const {
    if (resourceStatus == Qn::Offline) {
        return Qn::OfflineOverlay;
    } else if (resourceStatus == Qn::Unauthorized) {
        return Qn::UnauthorizedOverlay;
    } else if(m_renderStatus == Qn::NewFrameRendered) {
        return Qn::EmptyOverlay;
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
    return calculateStatusOverlay(m_resource->getStatus());
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

void QnResourceWidget::updateInfoVisiblity(bool animate)
{
    bool visible = isInfoVisible();

    qreal opacity = visible ? 1.0 : 0.0;

    if(animate) {
        opacityAnimator(m_footerWidget, 1.0)->animateTo(opacity);
    } else {
        m_footerWidget->setOpacity(opacity);
    }

    if(QnImageButtonWidget *infoButton = buttonBar()->button(InfoButton))
        infoButton->setChecked(visible);
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
    setOverlayVisible();
    m_mouseInWidget = true;

    base_type::hoverEnterEvent(event);
}

void QnResourceWidget::hoverMoveEvent(QGraphicsSceneHoverEvent *event) {
    if(!isOverlayVisible())
        setOverlayVisible();

    base_type::hoverMoveEvent(event);
}

void QnResourceWidget::hoverLeaveEvent(QGraphicsSceneHoverEvent *event) {
    if(!isInfoVisible())
        setOverlayVisible(false);
    m_mouseInWidget = false;

    base_type::hoverLeaveEvent(event);
}



void QnResourceWidget::optionsChangedNotify(Options changedFlags){
    if ((changedFlags & DisplayInfo) && (visibleButtons() & InfoButton))
        updateInfoVisiblity();
}

void QnResourceWidget::at_itemDataChanged(int role) {
    if (role != Qn::ItemCheckedButtonsRole)
        return;
    updateCheckedButtons();
}

void QnResourceWidget::at_iconButton_visibleChanged() {
    if(m_iconButton->isVisible()) {
        m_headerLayout->insertItem(0, m_iconButton);
    } else {
        m_headerLayout->removeItem(m_iconButton);
    }
}

void QnResourceWidget::at_infoButton_toggled(bool toggled) {
    setInfoVisible(toggled);
}

void QnResourceWidget::at_buttonBar_checkedButtonsChanged() {
    if (!item())
        return;

    item()->setData(Qn::ItemCheckedButtonsRole, static_cast<int>(checkedButtons()));
    update();
}
