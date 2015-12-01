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
#include <ui/graphics/items/controls/html_text_item.h>
#include <ui/graphics/items/standard/graphics_label.h>
#include <ui/graphics/items/generic/proxy_label.h>
#include <ui/graphics/items/generic/image_button_widget.h>
#include <ui/graphics/items/generic/image_button_bar.h>
#include <ui/graphics/items/generic/viewport_bound_widget.h>
#include <ui/graphics/items/overlays/resource_status_overlay_widget.h>
#include <ui/graphics/items/overlays/scrollable_overlay_widget.h>
#include <ui/workbench/workbench_item.h>
#include <ui/workbench/workbench_layout.h>
#include <ui/workbench/workbench_display.h>
#include <ui/workbench/workbench_access_controller.h>
#include <ui/style/globals.h>
#include <ui/style/skin.h>
#include <utils/aspect_ratio.h>

#include <utils/license_usage_helper.h>
#include <utils/common/string.h>

namespace {

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
    const QColor overlayBackgroundColor = QColor(0, 0, 0, 96); // TODO: #gdm #customization

    const QColor infoBackgroundColor = QColor(0, 0, 0, 127); // TODO: #gdm #customization

    const QColor overlayTextColor = QColor(255, 255, 255, 160); // TODO: #gdm #customization

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
        return (item && item->layout() && item->layout()->resource() && item->layout()->resource()->resourcePool());
    }

    GraphicsLabel *createGraphicsLabel() {
        auto label = new GraphicsLabel();
        label->setAcceptedMouseButtons(0);
        label->setPerformanceHint(GraphicsLabel::PixmapCaching);
        return label;
    }

    GraphicsWidget *createGraphicsWidget(QGraphicsLayout* layout) {
        auto widget = new GraphicsWidget();
        widget->setLayout(layout);
        widget->setAcceptedMouseButtons(0);
        widget->setAutoFillBackground(true);
        setPaletteColor(widget, QPalette::Window, overlayBackgroundColor);
        return widget;
    }

    QGraphicsLinearLayout *createGraphicsLayout(Qt::Orientation orientation) {
        auto layout = new QGraphicsLinearLayout(orientation);
        layout->setContentsMargins(0.0, 0.0, 0.0, 0.0);
        return layout;
    }

} // anonymous namespace

QnResourceWidget::OverlayWidgets::OverlayWidgets()
    : cameraNameOnlyOverlay(nullptr)
    , cameraNameOnlyLabel(createGraphicsLabel())

    , cameraNameWithButtonsOverlay(nullptr)
    , mainNameLabel(createGraphicsLabel())
    , mainExtrasLabel(createGraphicsLabel())

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
    m_overlayWidgets(),
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
    font.setPixelSize(20);
    setFont(font);
    setPaletteColor(this, QPalette::WindowText, overlayTextColor);

    createButtons();
    /* Handle layout permissions if an item is placed on the common layout. Otherwise, it can be Motion Widget, for example. */
    if (itemBelongsToValidLayout(item))
        connect(accessController()->notifier(item->layout()->resource()), &QnWorkbenchPermissionsNotifier::permissionsChanged, this, &QnResourceWidget::updateButtonsVisibility);

    addInfoOverlay();
    addMainOverlay();

    /* Status overlay. */
    m_statusOverlayWidget = new QnStatusOverlayWidget(m_resource, this);
    addOverlayWidget(m_statusOverlayWidget, UserVisible, true, false, StatusLayer);


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
    setInfoVisible(buttonBar()->button(InfoButton)->isChecked(), false);
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


void QnResourceWidget::addInfoOverlay() {
    {
        auto titleLayout = createGraphicsLayout(Qt::Horizontal);
        titleLayout->addItem(m_overlayWidgets.cameraNameOnlyLabel);
        titleLayout->addStretch(); /* Set large enough stretch for the buttons to be placed at the right end of the layout. */

        auto titleWidget = createGraphicsWidget(titleLayout);
        auto overlayLayout = createGraphicsLayout(Qt::Vertical);
        overlayLayout->addItem(titleWidget);
        overlayLayout->addStretch();

        QnViewportBoundWidget *cameraNameOnlyOverlay = new QnViewportBoundWidget(this);
        cameraNameOnlyOverlay->setLayout(overlayLayout);
        cameraNameOnlyOverlay->setAcceptedMouseButtons(0);
        m_overlayWidgets.cameraNameOnlyOverlay = cameraNameOnlyOverlay;

        addOverlayWidget(m_overlayWidgets.cameraNameOnlyOverlay, UserVisible, true, true, InfoLayer);
        setOverlayWidgetVisible(m_overlayWidgets.cameraNameOnlyOverlay, false, false);
    }

    {
        QnHtmlTextItemOptions infoOptions;
        infoOptions.backgroundColor = QColor(0, 0, 0, 127);
        infoOptions.borderRadius = 4;
        infoOptions.autosize = true;

        enum { kMargin = 2 };

        m_overlayWidgets.detailsItem->setOptions(infoOptions);
        m_overlayWidgets.detailsItem->setProperty(Qn::NoBlockMotionSelection, true);
        auto detailsOverlay = new QnScrollableOverlayWidget(Qt::AlignLeft, this);
        detailsOverlay->setProperty(Qn::NoBlockMotionSelection, true);
        detailsOverlay->setContentsMargins(kMargin, 0, 0, kMargin);
        detailsOverlay->addItem(m_overlayWidgets.detailsItem);
        addOverlayWidget(detailsOverlay, UserVisible, true, true, InfoLayer);
        m_overlayWidgets.detailsOverlay = detailsOverlay;
        setOverlayWidgetVisible(m_overlayWidgets.detailsOverlay, false, false);


        m_overlayWidgets.positionItem->setOptions(infoOptions);
        m_overlayWidgets.positionItem->setProperty(Qn::NoBlockMotionSelection, true);
        auto positionOverlay = new QnScrollableOverlayWidget(Qt::AlignRight, this);
        positionOverlay->setProperty(Qn::NoBlockMotionSelection, true);
        positionOverlay->setContentsMargins(0, 0, kMargin, kMargin);
        positionOverlay->addItem(m_overlayWidgets.positionItem);
        addOverlayWidget(positionOverlay, UserVisible, true, true, InfoLayer);
        m_overlayWidgets.positionOverlay = positionOverlay;
        setOverlayWidgetVisible(m_overlayWidgets.positionOverlay, false, false);
    }
}

void QnResourceWidget::addMainOverlay() {
    auto headerLayout = createGraphicsLayout(Qt::Horizontal);
    headerLayout->setSpacing(2.0);
    headerLayout->addItem(m_overlayWidgets.mainNameLabel);
    headerLayout->addStretch();
    headerLayout->addItem(m_overlayWidgets.mainExtrasLabel);
    headerLayout->addItem(m_buttonBar);

    connect(m_iconButton, &QnImageButtonWidget::visibleChanged, this, [this, headerLayout](){
        if(m_iconButton->isVisible()) {
            headerLayout->insertItem(0, m_iconButton);
        } else {
            headerLayout->removeItem(m_iconButton);
        }
    });

    auto headerWidget = createGraphicsWidget(headerLayout);

    QGraphicsLinearLayout *overlayLayout = createGraphicsLayout(Qt::Vertical);
    overlayLayout->addItem(headerWidget);
    overlayLayout->addStretch();

    QnViewportBoundWidget *cameraNameWithButtonsOverlay = new QnViewportBoundWidget(this);
    cameraNameWithButtonsOverlay->setLayout(overlayLayout);
    cameraNameWithButtonsOverlay->setAcceptedMouseButtons(0);
    m_overlayWidgets.cameraNameWithButtonsOverlay = cameraNameWithButtonsOverlay;

    addOverlayWidget(m_overlayWidgets.cameraNameWithButtonsOverlay, UserVisible, true, true, InfoLayer);
    setOverlayWidgetVisible(m_overlayWidgets.cameraNameWithButtonsOverlay, false, false);
}

void QnResourceWidget::createButtons() {
    QnImageButtonWidget *closeButton = new QnImageButtonWidget();
    closeButton->setIcon(qnSkin->icon("item/close.png"));
    closeButton->setProperty(Qn::NoBlockMotionSelection, true);
    closeButton->setToolTip(tr("Close"));
    connect(closeButton, &QnImageButtonWidget::clicked, this, &QnResourceWidget::close);

    QnImageButtonWidget *infoButton = new QnImageButtonWidget();
    infoButton->setIcon(qnSkin->icon("item/info.png"));
    infoButton->setCheckable(true);
    infoButton->setChecked(item()->displayInfo());
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

QString QnResourceWidget::titleText() const {
    return m_overlayWidgets.mainNameLabel->text();
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

    m_overlayWidgets.mainNameLabel->setText(leftText);
    m_overlayWidgets.mainExtrasLabel->setText(rightText);

    m_overlayWidgets.cameraNameOnlyLabel->setText(leftText);
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
    const QString text = calculateDetailsText();
    m_overlayWidgets.detailsItem->setHtml(text);
    m_overlayWidgets.detailsItem->setVisible(!text.isEmpty());
}

QString QnResourceWidget::calculatePositionText() const {
    return QString();
}

void QnResourceWidget::updatePositionText() {
    const QString text = calculatePositionText();
    m_overlayWidgets.positionItem->setHtml(text);
    m_overlayWidgets.positionItem->setVisible(!text.isEmpty());
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

    if(itemBelongsToValidLayout(item())) {
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
        * there are two options: 'mouse in the widget' and 'info button checked'
        * camera name should be visible if any option is active
        * position item should be visible if any option is active
        * control buttons should be visible if mouse cursor is in the widget
        * detailed info should be visible if both options are active simultaneously (or runtime property set)
    */

    /* Motion mask widget should not have overlays at all */
    bool overlaysCanBeVisible = !options().testFlag(QnResourceWidget::InfoOverlaysForbidden);

    bool detailsVisible = m_options.testFlag(DisplayInfo);
    if(QnImageButtonWidget *infoButton = buttonBar()->button(InfoButton))
        infoButton->setChecked(detailsVisible);

    bool showOnlyCameraName         = overlaysCanBeVisible && detailsVisible && !m_mouseInWidget;
    bool showCameraNameWithButtons  = overlaysCanBeVisible && m_mouseInWidget;
    bool showPosition               = overlaysCanBeVisible && (detailsVisible || m_mouseInWidget);
    bool showDetailedInfo           = overlaysCanBeVisible && detailsVisible && (m_mouseInWidget || qnRuntime->showFullInfo());

    setOverlayWidgetVisible(m_overlayWidgets.cameraNameWithButtonsOverlay,  showCameraNameWithButtons,  animate);
    setOverlayWidgetVisible(m_overlayWidgets.cameraNameOnlyOverlay,         showOnlyCameraName,         animate);
    setOverlayWidgetVisible(m_overlayWidgets.positionOverlay,               showPosition,               animate);
    setOverlayWidgetVisible(m_overlayWidgets.detailsOverlay,                showDetailedInfo,           animate);
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

void QnResourceWidget::optionsChangedNotify(Options changedFlags){
    if (changedFlags.testFlag(DisplayInfo) && visibleButtons().testFlag(InfoButton))
        updateHud(false);
    else if (changedFlags.testFlag(InfoOverlaysForbidden))
        updateHud(false);
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

    item()->setData(Qn::ItemCheckedButtonsRole, static_cast<int>(checkedButtons()));
    update();
}

