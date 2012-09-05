#include "resource_widget.h"

#include <cassert>
#include <cmath>

#include <QtGui/QPainter>
#include <QtGui/QGraphicsLinearLayout>

#include <utils/common/warnings.h>
#include <utils/common/scoped_painter_rollback.h>
#include <utils/common/util.h>
#include <utils/common/synctime.h>

#include <core/resource/resource_media_layout.h>
#include <core/resource/security_cam_resource.h>
#include <core/resource/layout_resource.h>
#include <core/resourcemanagment/resource_pool.h>

#include <ui/common/color_transformations.h>
#include <ui/animation/opacity_animator.h>
#include <ui/graphics/opengl/gl_shortcuts.h>
#include <ui/graphics/opengl/gl_context_data.h>
#include <ui/graphics/painters/loading_progress_painter.h>
#include <ui/graphics/painters/paused_painter.h>
#include <ui/graphics/instruments/motion_selection_instrument.h>
#include <ui/graphics/items/standard/graphics_label.h>
#include <ui/graphics/items/generic/image_button_widget.h>
#include <ui/graphics/items/generic/image_button_bar.h>
#include <ui/graphics/items/generic/viewport_bound_widget.h>
#include <ui/workbench/workbench_item.h>
#include <ui/workbench/workbench_layout.h>
#include <ui/workbench/workbench_access_controller.h>
#include <ui/style/globals.h>
#include <ui/style/skin.h>

namespace {

    /** Flashing text flash interval */
    static const int TEXT_FLASHING_PERIOD = 1000;

    /** Frame extension multiplier determines the width of frame extension relative
     * to frame width.
     *
     * Frame events are processed not only when hovering over the frame itself,
     * but also over its extension. */
    const qreal frameExtensionMultiplier = 1.0;

    /** Default shadow displacement, in scene coordinates. */
    const QPointF defaultShadowDisplacement = QPointF(qnGlobals->workbenchUnitSize(), qnGlobals->workbenchUnitSize()) * 0.05;

    /** Default timeout before the video is displayed as "loading", in milliseconds. */
    const qint64 defaultLoadingTimeoutMSec = MAX_FRAME_DURATION;

    /** Default period of progress circle. */
    const qint64 defaultProgressPeriodMSec = 1000;

    /** Default duration of "fade-in" effect for overlay icons. */
    const qint64 defaultOverlayFadeInDurationMSec = 500;

    /** Default size of widget header buttons, in pixels. */
    const QSizeF headerButtonSize = QSizeF(24, 24);

    /** Background color for overlay panels. */
    const QColor overlayBackgroundColor = QColor(0, 0, 0, 96);

    const QColor overlayTextColor = QColor(255, 255, 255, 160);

    const QColor selectedFrameColor = qnGlobals->selectedFrameColor();

    const QColor frameColor = qnGlobals->frameColor();

    class QnLoadingProgressPainterFactory {
    public:
        QnLoadingProgressPainter *operator()(const QGLContext *context) {
            return new QnLoadingProgressPainter(0.5, 12, 0.5, QColor(255, 255, 255, 0), QColor(255, 255, 255, 255), context);
        }
    };

    typedef QnGlContextData<QnLoadingProgressPainter, QnLoadingProgressPainterFactory> QnLoadingProgressPainterStorage;
    Q_GLOBAL_STATIC(QnLoadingProgressPainterStorage, qn_resourceWidget_loadingProgressPainterStorage);

    Q_GLOBAL_STATIC(QnGlContextData<QnPausedPainter>, qn_resourceWidget_pausedPainterStorage);

    Q_GLOBAL_STATIC(QnDefaultDeviceVideoLayout, qn_resourceWidget_defaultContentLayout);

} // anonymous namespace


// -------------------------------------------------------------------------- //
// Logic
// -------------------------------------------------------------------------- //
QnResourceWidget::QnResourceWidget(QnWorkbenchContext *context, QnWorkbenchItem *item, QGraphicsItem *parent):
    base_type(parent),
    QnWorkbenchContextAware(context),
    m_item(item),
    m_channelsLayout(NULL),
    m_aspectRatio(-1.0),
    m_enclosingAspectRatio(1.0),
    m_frameWidth(-1.0),
    m_frameOpacity(1.0),
    m_aboutToBeDestroyedEmitted(false),
    m_displayFlags(DisplaySelectionOverlay | DisplayButtons)
{
    setAcceptHoverEvents(true);

    /* Set up shadow. */
    shadowItem()->setColor(qnGlobals->shadowColor());
    setShadowDisplacement(defaultShadowDisplacement);

    /* Set up frame. */
    setFrameWidth(0.0);

    /* Set up overlay widgets. */
    QFont font = this->font();
    font.setPixelSize(20);
    setFont(font);
    {
        QPalette palette = this->palette();
        palette.setColor(QPalette::WindowText, overlayTextColor);
        setPalette(palette);
    }

    /* Header overlay. */
    m_headerLabel = new GraphicsLabel();
    m_headerLabel->setAcceptedMouseButtons(0);
    m_headerLabel->setPerformanceHint(QStaticText::AggressiveCaching);

#if 0
    QnImageButtonWidget *togglePinButton = new QnImageButtonWidget();
    togglePinButton->setIcon(Skin::icon("decorations/pin.png", "decorations/unpin.png"));
    togglePinButton->setCheckable(true);
    togglePinButton->setChecked(item->isPinned());
    togglePinButton->setPreferredSize(headerButtonSize);
    connect(togglePinButton, SIGNAL(clicked()), item, SLOT(togglePinned()));
    headerLayout->addItem(togglePinButton);
#endif

    QnImageButtonWidget *closeButton = new QnImageButtonWidget();
    closeButton->setIcon(qnSkin->icon("decorations/item_close.png"));
    closeButton->setProperty(Qn::NoBlockMotionSelection, true);
    connect(closeButton, SIGNAL(clicked()), this, SLOT(close()));
    connect(accessController()->notifier(item->layout()->resource()), SIGNAL(permissionsChanged(const QnResourcePtr &)), this, SLOT(updateButtonsVisibility()));

    QnImageButtonWidget *infoButton = new QnImageButtonWidget();
    infoButton->setIcon(qnSkin->icon("decorations/item_info.png"));
    infoButton->setCheckable(true);
    infoButton->setProperty(Qn::NoBlockMotionSelection, true);
    connect(infoButton, SIGNAL(toggled(bool)), this, SLOT(setInfoVisible(bool)));
    
    QnImageButtonWidget *rotateButton = new QnImageButtonWidget();
    rotateButton->setIcon(qnSkin->icon("decorations/item_rotate.png"));
    rotateButton->setProperty(Qn::NoBlockMotionSelection, true);
    connect(rotateButton, SIGNAL(pressed()), this, SIGNAL(rotationStartRequested()));
    connect(rotateButton, SIGNAL(released()), this, SIGNAL(rotationStopRequested()));

    m_buttonBar = new QnImageButtonBar();
    m_buttonBar->setUniformButtonSize(QSizeF(24.0, 24.0));
    m_buttonBar->addButton(CloseButton, closeButton);
    m_buttonBar->addButton(InfoButton, infoButton);
    m_buttonBar->addButton(RotateButton, rotateButton);

    QGraphicsLinearLayout *headerLayout = new QGraphicsLinearLayout(Qt::Horizontal);
    headerLayout->setContentsMargins(0.0, 0.0, 0.0, 0.0);
    headerLayout->setSpacing(2.0);
    headerLayout->addItem(m_headerLabel);
    headerLayout->addStretch(0x1000); /* Set large enough stretch for the buttons to be placed at the right end of the layout. */
    headerLayout->addItem(m_buttonBar);

    m_headerWidget = new QGraphicsWidget();
    m_headerWidget->setLayout(headerLayout);
    m_headerWidget->setAcceptedMouseButtons(0);
    m_headerWidget->setAutoFillBackground(true);
    {
        QPalette palette = m_headerWidget->palette();
        palette.setColor(QPalette::Window, overlayBackgroundColor);
        m_headerWidget->setPalette(palette);
    }

    QGraphicsLinearLayout *headerOverlayLayout = new QGraphicsLinearLayout(Qt::Vertical);
    headerOverlayLayout->setContentsMargins(0.0, 0.0, 0.0, 0.0);
    headerOverlayLayout->addItem(m_headerWidget);
    headerOverlayLayout->addStretch(0x1000);

    m_headerOverlayWidget = new QnViewportBoundWidget(this);
    m_headerOverlayWidget->setLayout(headerOverlayLayout);
    m_headerOverlayWidget->setAcceptedMouseButtons(0);
    m_headerOverlayWidget->setOpacity(0.0);

    /* Footer overlay. */
    m_footerLeftLabel = new GraphicsLabel();
    m_footerLeftLabel->setAcceptedMouseButtons(0);

    m_footerRightLabel = new GraphicsLabel();
    m_footerRightLabel->setAcceptedMouseButtons(0);

    QGraphicsLinearLayout *footerLayout = new QGraphicsLinearLayout(Qt::Horizontal);
    footerLayout->setContentsMargins(0.0, 0.0, 0.0, 0.0);
    footerLayout->addItem(m_footerLeftLabel);
    footerLayout->addStretch(0x1000);
    footerLayout->addItem(m_footerRightLabel);

    m_footerWidget = new QGraphicsWidget();
    m_footerWidget->setLayout(footerLayout);
    m_footerWidget->setAcceptedMouseButtons(0);
    m_footerWidget->setAutoFillBackground(true);
    {
        QPalette palette = m_footerWidget->palette();
        palette.setColor(QPalette::Window, overlayBackgroundColor);
        m_footerWidget->setPalette(palette);
    }
    m_footerWidget->setOpacity(0.0);

    QGraphicsLinearLayout *footerOverlayLayout = new QGraphicsLinearLayout(Qt::Vertical);
    footerOverlayLayout->setContentsMargins(0.0, 0.0, 0.0, 0.0);
    footerOverlayLayout->addStretch(0x1000);
    footerOverlayLayout->addItem(m_footerWidget);

    m_footerOverlayWidget = new QnViewportBoundWidget(this);
    m_footerOverlayWidget->setLayout(footerOverlayLayout);
    m_footerOverlayWidget->setAcceptedMouseButtons(0);
    m_footerOverlayWidget->setOpacity(0.0);


    /* Initialize resource. */
    m_resource = qnResPool->getEnabledResourceByUniqueId(item->resourceUid());
    if(!m_resource)
        m_resource = qnResPool->getResourceByUniqId(item->resourceUid());
    connect(m_resource.data(), SIGNAL(nameChanged()), this, SLOT(updateTitleText()));
    setChannelLayout(qn_resourceWidget_defaultContentLayout());


    /* Init static text. */
    m_noDataStaticText.setText(tr("NO DATA"));
    m_noDataStaticText.setPerformanceHint(QStaticText::AggressiveCaching);
    m_offlineStaticText.setText(tr("NO SIGNAL"));
    m_offlineStaticText.setPerformanceHint(QStaticText::AggressiveCaching);
    m_unauthorizedStaticText.setText(tr("Unauthorized"));
    m_unauthorizedStaticText.setPerformanceHint(QStaticText::AggressiveCaching);
    m_unauthorizedStaticText2.setText(tr("Please check authentication information in camera settings"));
    m_unauthorizedStaticText2.setPerformanceHint(QStaticText::AggressiveCaching);


    /* Run handlers. */
    updateTitleText();
    updateButtonsVisibility();
}

QnResourceWidget::~QnResourceWidget() {
    ensureAboutToBeDestroyedEmitted();
}

QnResourcePtr QnResourceWidget::resource() const {
    return m_resource;
}

void QnResourceWidget::setFrameWidth(qreal frameWidth) {
    if(qFuzzyCompare(m_frameWidth, frameWidth))
        return;

    prepareGeometryChange();

    m_frameWidth = frameWidth;
    qreal extendedFrameWidth = m_frameWidth * (1.0 + frameExtensionMultiplier);
    setWindowFrameMargins(extendedFrameWidth, extendedFrameWidth, extendedFrameWidth, extendedFrameWidth);

    invalidateShadowShape();
    if(shadowItem())
        shadowItem()->setSoftWidth(m_frameWidth);
}

void QnResourceWidget::setAspectRatio(qreal aspectRatio) {
    if(qFuzzyCompare(m_aspectRatio, aspectRatio))
        return;

    QRectF enclosingGeometry = this->enclosingGeometry();
    m_aspectRatio = aspectRatio;

    updateGeometry(); /* Discard cached size hints. */
    setGeometry(expanded(m_aspectRatio, enclosingGeometry, Qt::KeepAspectRatio));

    emit aspectRatioChanged();
}

void QnResourceWidget::setEnclosingAspectRatio(qreal enclosingAspectRatio) {
    m_enclosingAspectRatio = enclosingAspectRatio;
}

QRectF QnResourceWidget::enclosingGeometry() const {
    return expanded(m_enclosingAspectRatio, geometry(), Qt::KeepAspectRatioByExpanding);
}

void QnResourceWidget::setEnclosingGeometry(const QRectF &enclosingGeometry) {
    m_enclosingAspectRatio = enclosingGeometry.width() / enclosingGeometry.height();

    if(hasAspectRatio()) {
        setGeometry(expanded(m_aspectRatio, enclosingGeometry, Qt::KeepAspectRatio));
    } else {
        setGeometry(enclosingGeometry);
    }
}

void QnResourceWidget::setGeometry(const QRectF &geometry) {
    base_type::setGeometry(geometry);
    setTransformOriginPoint(rect().center());

    m_headerOverlayWidget->setDesiredSize(size());
    m_footerOverlayWidget->setDesiredSize(size());
}

QString QnResourceWidget::titleText() const {
    return m_headerLabel->text();
}

void QnResourceWidget::setTitleText(const QString &titleText) {
    if(m_titleText == titleText)
        return;

    m_titleText = titleText;

    updateTitleText();
}

void QnResourceWidget::setTitleTextInternal(const QString &titleText) {
    m_headerLabel->setText(titleText);
}

QString QnResourceWidget::calculateTitleText() const {
    return m_resource->getName();
}

void QnResourceWidget::updateTitleText() {
    setTitleTextInternal(m_titleText.isNull() ? calculateTitleText() : m_titleText);
}

void QnResourceWidget::setInfoText(const QString &infoText) {
    if(m_infoText == infoText)
        return;

    m_infoText = infoText;

    updateInfoText();
}

QString QnResourceWidget::infoText() {
    QString leftText = m_footerLeftLabel->text();
    QString rightText = m_footerRightLabel->text();
    
    return rightText.isEmpty() ? leftText : (leftText + QLatin1Char('\t') + rightText);
}

void QnResourceWidget::setInfoTextInternal(const QString &infoText) {
    QString leftText = infoText;
    QString rightText;

    int index = leftText.indexOf(QLatin1Char('\t'));
    if(index != -1) {
        rightText = leftText.mid(index + 1);
        leftText = leftText.mid(0, index);
    }

    m_footerLeftLabel->setText(leftText);
    m_footerRightLabel->setText(rightText);
}

QString QnResourceWidget::calculateInfoText() const {
    return QString();
}

void QnResourceWidget::updateInfoText() {
    setInfoTextInternal(m_infoText.isNull() ? calculateInfoText() : m_infoText);
}

QSizeF QnResourceWidget::constrainedSize(const QSizeF constraint) const {
    if(!hasAspectRatio())
        return constraint;

    return expanded(m_aspectRatio, constraint, Qt::KeepAspectRatio);
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
    if (m_channelsLayout->numberOfChannels() == 1)
        return QRectF(QPointF(0.0, 0.0), size());

    QSizeF size = this->size();
    qreal w = size.width() / m_channelsLayout->width();
    qreal h = size.height() / m_channelsLayout->height();

    return QRectF(
        w * m_channelsLayout->h_position(channel),
        h * m_channelsLayout->v_position(channel),
        w,
        h
    );
}

Qn::RenderStatus QnResourceWidget::channelRenderStatus(int channel) const {
    return m_channelState[channel].renderStatus;
}

bool QnResourceWidget::isDecorationsVisible() const {
    return !qFuzzyIsNull(m_headerOverlayWidget->opacity()); /* Note that it's OK to check only header opacity here. */
}

void QnResourceWidget::setDecorationsVisible(bool visible, bool animate) {
    qreal opacity = visible ? 1.0 : 0.0;

    if(animate) {
        opacityAnimator(m_footerOverlayWidget, 1.0)->animateTo(opacity);
        opacityAnimator(m_headerOverlayWidget, 1.0)->animateTo(opacity);
    } else {
        m_footerOverlayWidget->setOpacity(opacity);
        m_headerOverlayWidget->setOpacity(opacity);
    }
}

bool QnResourceWidget::isInfoVisible() const {
    return !qFuzzyIsNull(m_footerWidget->opacity());
}

void QnResourceWidget::setInfoVisible(bool visible, bool animate) {
    qreal opacity = visible ? 1.0 : 0.0;

    if(animate) {
        opacityAnimator(m_footerWidget, 1.0)->animateTo(opacity);
    } else {
        m_footerWidget->setOpacity(opacity);
    }

    if(QnImageButtonWidget *infoButton = buttonBar()->button(InfoButton))
        infoButton->setChecked(visible);
}

Qt::WindowFrameSection QnResourceWidget::windowFrameSectionAt(const QPointF &pos) const {
    return Qn::toQtFrameSection(static_cast<Qn::WindowFrameSection>(static_cast<int>(windowFrameSectionsAt(QRectF(pos, QSizeF(0.0, 0.0))))));
}

Qn::WindowFrameSections QnResourceWidget::windowFrameSectionsAt(const QRectF &region) const {
    Qn::WindowFrameSections result = Qn::calculateRectangularFrameSections(windowFrameRect(), rect(), region);

    /* This widget has no side frame sections in case aspect ratio is set. */
    if(hasAspectRatio())
        result = result & ~(Qn::LeftSection | Qn::RightSection | Qn::TopSection | Qn::BottomSection);

    return result;
}

void QnResourceWidget::ensureAboutToBeDestroyedEmitted() {
    if(m_aboutToBeDestroyedEmitted)
        return;

    m_aboutToBeDestroyedEmitted = true;
    emit aboutToBeDestroyed();
}

void QnResourceWidget::setDisplayFlags(DisplayFlags flags) {
    if(m_displayFlags == flags)
        return;

    DisplayFlags changedFlags = m_displayFlags ^ flags;
    m_displayFlags = flags;

    if(changedFlags & DisplayButtons)
        m_headerOverlayWidget->setVisible(flags & DisplayButtons);

    displayFlagsChangedNotify(changedFlags);
    emit displayFlagsChanged();
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

QnResourceWidget::Buttons QnResourceWidget::calculateButtonsVisibility() const {
    Buttons result = InfoButton | RotateButton;

    if(item() && item()->layout()) {
        Qn::Permissions requiredPermissions = Qn::WritePermission | Qn::AddRemoveItemsPermission;
        if((accessController()->permissions(item()->layout()->resource()) & requiredPermissions) == requiredPermissions)
            result |= CloseButton;
    }

    return result;
}

void QnResourceWidget::updateButtonsVisibility() {
    m_buttonBar->setVisibleButtons(calculateButtonsVisibility());
}

QnResourceWidget::Overlay QnResourceWidget::channelOverlay(int channel) const {
    return m_channelState[channel].overlay;
}

void QnResourceWidget::setChannelOverlay(int channel, Overlay overlay) {
    ChannelState &state = m_channelState[channel];
    if(state.overlay == overlay)
        return;

    state.fadeInNeeded = state.overlay == EmptyOverlay;
    state.changeTimeMSec = QDateTime::currentMSecsSinceEpoch();
    state.overlay = overlay;
}

QnResourceWidget::Overlay QnResourceWidget::calculateChannelOverlay(int channel, int resourceStatus) const {
    if (resourceStatus == QnResource::Offline) {
        return OfflineOverlay;
    } else if (resourceStatus == QnResource::Unauthorized) {
        return UnauthorizedOverlay;
    } else {
        Qn::RenderStatus renderStatus = m_channelState[channel].renderStatus;
        qint64 currentTimeMSec = QDateTime::currentMSecsSinceEpoch();

        if(renderStatus != Qn::NewFrameRendered && (renderStatus != Qn::OldFrameRendered || currentTimeMSec - m_channelState[channel].lastNewFrameTimeMSec >= defaultLoadingTimeoutMSec)) {
            return LoadingOverlay;
        } else {
            return EmptyOverlay;
        }
    }
}

QnResourceWidget::Overlay QnResourceWidget::calculateChannelOverlay(int channel) const {
    return calculateChannelOverlay(channel, m_resource->getStatus());
}

void QnResourceWidget::updateChannelOverlay(int channel) {
    setChannelOverlay(channel, calculateChannelOverlay(channel));
}

void QnResourceWidget::setChannelLayout(const QnVideoResourceLayout *channelLayout) {
    if(m_channelsLayout == channelLayout)
        return;

    m_channelsLayout = channelLayout;

    m_channelState.resize(m_channelsLayout->numberOfChannels());
    channelLayoutChangedNotify();
}

int QnResourceWidget::channelCount() const {
    return m_channelsLayout->numberOfChannels();
}


// -------------------------------------------------------------------------- //
// Painting
// -------------------------------------------------------------------------- //
void QnResourceWidget::paint(QPainter *painter, const QStyleOptionGraphicsItem * /*option*/, QWidget * /*widget*/) {
    if (painter->paintEngine() == NULL) {
        qnWarning("No OpenGL-compatible paint engine was found.");
        return;
    }

    if (painter->paintEngine()->type() != QPaintEngine::OpenGL2 && painter->paintEngine()->type() != QPaintEngine::OpenGL) {
        qnWarning("Painting with the paint engine of type '%1' is not supported", static_cast<int>(painter->paintEngine()->type()));
        return;
    }

    if(m_pausedPainter.isNull()) {
        m_pausedPainter = qn_resourceWidget_pausedPainterStorage()->get(QGLContext::currentContext());
        m_loadingProgressPainter = qn_resourceWidget_loadingProgressPainterStorage()->get(QGLContext::currentContext());
    }

    QnScopedPainterPenRollback penRollback(painter);
    QnScopedPainterBrushRollback brushRollback(painter);
    QnScopedPainterFontRollback fontRollback(painter);

    /* Update screen size of a single channel. */
    setChannelScreenSize(painter->combinedTransform().mapRect(channelRect(0)).size().toSize());

    qint64 currentTimeMSec = QDateTime::currentMSecsSinceEpoch();

    for(int i = 0; i < channelCount(); i++) {
        /* Draw content. */
        QRectF rect = channelRect(i);
        Qn::RenderStatus renderStatus = paintChannelBackground(painter, i, rect);

        /* Update channel state. */
        m_channelState[i].renderStatus = renderStatus;
        if(renderStatus == Qn::NewFrameRendered)
            m_channelState[i].lastNewFrameTimeMSec = currentTimeMSec;
        updateChannelOverlay(i);

        /* Draw overlay icon. */
        qreal overlayOpacity = 1.0;
        if(m_channelState[i].fadeInNeeded)
            overlayOpacity *= qBound(0.0, static_cast<qreal>(currentTimeMSec - m_channelState[i].changeTimeMSec) / defaultOverlayFadeInDurationMSec, 1.0);
        
        qreal opacity = painter->opacity();
        painter->setOpacity(opacity * overlayOpacity);
        paintOverlay(painter, rect, m_channelState[i].overlay);
        painter->setOpacity(opacity);

        /* Draw foreground. */
        paintChannelForeground(painter, i, rect);

        /* Draw selected / not selected overlay. */
        paintSelection(painter, rect);
    }
}

void QnResourceWidget::paintChannelForeground(QPainter *, int, const QRectF &) {
    return;
}

void QnResourceWidget::paintWindowFrame(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *) {
    if(qFuzzyIsNull(m_frameOpacity))
        return;

    QSizeF size = this->size();
    qreal w = size.width();
    qreal h = size.height();
    qreal fw = m_frameWidth;
    QColor color = isSelected() ? selectedFrameColor : frameColor;

    QnScopedPainterOpacityRollback opacityRollback(painter, painter->opacity() * m_frameOpacity);
    QnScopedPainterAntialiasingRollback antialiasingRollback(painter, true); /* Antialiasing is here for a reason. Without it border looks crappy. */
    painter->fillRect(QRectF(-fw,     -fw,     w + fw * 2,  fw), color);
    painter->fillRect(QRectF(-fw,     h,       w + fw * 2,  fw), color);
    painter->fillRect(QRectF(-fw,     0,       fw,          h),  color);
    painter->fillRect(QRectF(w,       0,       fw,          h),  color);
}

void QnResourceWidget::paintFlashingText(QPainter *painter, const QStaticText &text, qreal textSize, const QPointF &offset) {
    qreal unit = qnGlobals->workbenchUnitSize(); //channelRect(0).width();

    QFont font;
    font.setPointSizeF(textSize * unit);
    font.setStyleHint(QFont::SansSerif, QFont::ForceOutline);
    QnScopedPainterFontRollback fontRollback(painter, font);
    QnScopedPainterPenRollback penRollback(painter, QPen(QColor(255, 208, 208, 196)));

    qreal opacity = painter->opacity();
    painter->setOpacity(opacity * qAbs(std::sin(QDateTime::currentMSecsSinceEpoch() / qreal(TEXT_FLASHING_PERIOD * 2) * M_PI)));
    painter->drawStaticText(rect().center() - toPoint(text.size() / 2) + offset * unit, text);
    painter->setOpacity(opacity);
}

void QnResourceWidget::paintSelection(QPainter *painter, const QRectF &rect) {
    if(!isSelected())
        return;

    if(!(m_displayFlags & DisplaySelectionOverlay))
        return;

    painter->fillRect(rect, qnGlobals->selectionColor());
}

void QnResourceWidget::paintOverlay(QPainter *painter, const QRectF &rect, Overlay overlay) {
    if(overlay == EmptyOverlay)
        return;

    painter->fillRect(rect, QColor(0, 0, 0, 128));

    if(overlay == LoadingOverlay || overlay == PausedOverlay) {
        qint64 currentTimeMSec = QDateTime::currentMSecsSinceEpoch();

        painter->beginNativePainting();
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        QRectF overlayRect = expanded(
            1.0,
            QRectF(
                rect.center() - toPoint(rect.size()) / 8,
                rect.size() / 4
            ),
            Qt::KeepAspectRatio
        );

        glPushMatrix();
        glTranslatef(overlayRect.center().x(), overlayRect.center().y(), 1.0);
        glScalef(overlayRect.width() / 2, overlayRect.height() / 2, 1.0);
        if(overlay == LoadingOverlay) {
            m_loadingProgressPainter->paint(
                static_cast<qreal>(currentTimeMSec % defaultProgressPeriodMSec) / defaultProgressPeriodMSec,
                painter->opacity()
            );
        } else if(overlay == PausedOverlay) {
            m_pausedPainter->paint(0.5 * painter->opacity());
        }
        glPopMatrix();

        glDisable(GL_BLEND);
        painter->endNativePainting();
    }

    if (overlay == NoDataOverlay) {
        paintFlashingText(painter, m_noDataStaticText, 0.1);
    } else if (overlay == OfflineOverlay) {
        paintFlashingText(painter, m_offlineStaticText, 0.1);
    } else if (overlay == UnauthorizedOverlay) {
        paintFlashingText(painter, m_unauthorizedStaticText, 0.1);
        paintFlashingText(painter, m_unauthorizedStaticText2, 0.025, QPointF(0.0, 0.1));
    }

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
            unsetCursor();
    }

    return result;
}

void QnResourceWidget::hoverEnterEvent(QGraphicsSceneHoverEvent *event) {
    setDecorationsVisible(true);

    base_type::hoverEnterEvent(event);
}

void QnResourceWidget::hoverMoveEvent(QGraphicsSceneHoverEvent *event) {
    if(!isDecorationsVisible())
        setDecorationsVisible(true);

    base_type::hoverMoveEvent(event);
}

void QnResourceWidget::hoverLeaveEvent(QGraphicsSceneHoverEvent *event) {
    if(!isInfoVisible())
        setDecorationsVisible(false);

    base_type::hoverLeaveEvent(event);
}



