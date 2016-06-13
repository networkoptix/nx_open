#include "resource_status_overlay_widget.h"

#include <cmath> /* For std::sin. */

#include <QtCore/QDateTime>
#include <QtCore/QtMath>

#include <core/resource/device_dependent_strings.h>
#include <core/resource/camera_resource.h>

#include <utils/common/scoped_painter_rollback.h>

#include <ui/animation/opacity_animator.h>
#include <ui/graphics/items/generic/text_button_widget.h>
#include <ui/graphics/painters/loading_progress_painter.h>
#include <ui/graphics/painters/paused_painter.h>
#include <ui/graphics/opengl/gl_context_data.h>
#include <ui/style/globals.h>
#include <ui/workaround/gl_native_painting.h>
#include "opengl_renderer.h"
#include "ui/style/skin.h"
#include "utils/math/color_transformations.h"
#include <ui/common/text_pixmap_cache.h>

#include <ui/workaround/sharp_pixmap_painting.h>

/** @def QN_RESOURCE_WIDGET_FLASHY_LOADING_OVERLAY
 *
 * When defined, makes loading overlay much more 'flashy', drawing loading circles
 * and reducing timeout after which overlay is drawn.
 */
// #define QN_RESOURCE_WIDGET_FLASHY_LOADING_OVERLAY

namespace {

    class QnLoadingProgressPainterFactory {
    public:
        QnLoadingProgressPainter *operator()(const QGLContext *context) {
            return new QnLoadingProgressPainter(0.5, 12, 0.5, QColor(255, 255, 255, 0), QColor(255, 255, 255, 255), context);
        }
    };

    typedef QnGlContextData<QnLoadingProgressPainter, QnLoadingProgressPainterFactory> QnLoadingProgressPainterStorage;
    Q_GLOBAL_STATIC(QnLoadingProgressPainterStorage, qn_loadingProgressPainterStorage);

    Q_GLOBAL_STATIC(QnGlContextData<QnPausedPainter>, qn_pausedPainterStorage);

    const qreal kStaticFontSize = 50;

    const int kFlashingPeriodMs = 1000;

} // anonymous namespace

QnStatusOverlayWidget::QnStatusOverlayWidget(const QnResourcePtr &resource, QGraphicsWidget *parent, Qt::WindowFlags windowFlags)
    : base_type(parent, windowFlags)
    , m_resource(resource)
    , m_statusOverlay(Qn::EmptyOverlay)
    , m_buttonType(NoButton)
    , m_button(new QnTextButtonWidget(lit("resource_status_overlay_do_action"), this))
{
    setAcceptedMouseButtons(0);

    QnVirtualCameraResourcePtr camera = m_resource.dynamicCast<QnVirtualCameraResource>();

    /* Init static text. */
    m_staticFont.setPointSizeF(kStaticFontSize);
    m_staticFont.setStyleHint(QFont::SansSerif, QFont::ForceOutline);

    m_texts[NoDataText] = tr("NO DATA");
    m_texts[OfflineText] = tr("NO SIGNAL");
    m_texts[ServerOfflineText] = tr("Server Offline");
    m_texts[UnauthorizedText] = tr("Unauthorized");
    m_texts[UnauthorizedSubText] = QnDeviceDependentStrings::getNameFromSet(
            QnCameraDeviceStringSet(
                tr("Please check authentication information in device settings"),
                tr("Please check authentication information in camera settings"),
                tr("Please check authentication information in I/O module settings")
            ), camera
        );
    m_texts[AnalogLicenseText] = tr("Activate analog license to remove this message");
    m_texts[VideowallLicenseText] = tr("Activate Video Wall license to remove this message");
    m_texts[LoadingText] = tr("Loading...");
    m_texts[IoModuleDisabledText] = tr("Module is disabled");

    /* Init button. */
    m_button->setObjectName(lit("statusOverlayButton"));
    m_button->setFrameShape(Qn::RectangularFrame);
    m_button->setRelativeFrameWidth(1.0 / 16.0);
    m_button->setOpacity(0);
    m_button->setStateOpacity(0, 0.4);
    m_button->setStateOpacity(QnImageButtonWidget::Hovered, 0.7);
    m_button->setStateOpacity(QnImageButtonWidget::Pressed, 1.0);
    m_button->setFont(m_staticFont);

    connect(m_button, &QnTextButtonWidget::clicked, this, [&](){
        switch (m_buttonType) {
        case DiagnosticsButton:
            emit diagnosticsRequested();
            break;
        case IoEnableButton:
            emit ioEnableRequested();
            break;
        case MoreLicensesButton:
            emit moreLicensesRequested();
            break;
        default:
            break;
        }
    });

    updateLayout();
    updateButtonOpacity(false);
}

QnStatusOverlayWidget::~QnStatusOverlayWidget() {
    return;
}

Qn::ResourceStatusOverlay QnStatusOverlayWidget::statusOverlay() const {
    return m_statusOverlay;
}

void QnStatusOverlayWidget::setStatusOverlay(Qn::ResourceStatusOverlay statusOverlay) {
    if(m_statusOverlay == statusOverlay)
        return;

    m_statusOverlay = statusOverlay;

    updateButtonOpacity();

    emit statusOverlayChanged();
}

void QnStatusOverlayWidget::updateLayout() {
    QRectF rect = this->rect();

    qreal point = qMin(rect.width(), rect.height()) / 16.0;
    QSizeF size(point * 6.0, point * 1.5);

    m_button->setGeometry(QRectF(rect.center() - toPoint(size) / 2.0 + QPointF(0.0, point * 4.0), size));
}

void QnStatusOverlayWidget::setButtonVisible(bool visible
    , bool animate)
{
    static const char kVisiblePropertyName[] = "isVisible";
    const bool currentVisible = m_button->property(kVisiblePropertyName).toBool();

    if (currentVisible == visible)
        return;

    m_button->setProperty(kVisiblePropertyName, visible);

    const qreal buttonOpacity = (visible ? 1.0 : 0.0);
    if (animate)
        opacityAnimator(m_button)->animateTo(buttonOpacity);
    else
        m_button->setOpacity(buttonOpacity);
}

void QnStatusOverlayWidget::updateButtonOpacity(bool animate) {
    bool visible = false;

    switch (m_buttonType) {
    case DiagnosticsButton:
        visible = m_statusOverlay == Qn::OfflineOverlay;
        break;
    case IoEnableButton:
    case MoreLicensesButton:
        visible = m_statusOverlay == Qn::IoModuleDisabledOverlay;
        break;
    default:
        break;
    }

    setButtonVisible(visible, animate);
}

void QnStatusOverlayWidget::updateButtonText() {
    switch (m_buttonType) {
    case DiagnosticsButton:
        m_button->setText(tr("Diagnostics..."));
        break;
    case IoEnableButton:
        m_button->setText(tr("Enable"));
        break;
    case MoreLicensesButton:
        m_button->setText(tr("Activate license..."));
        break;
    default:
        break;
    }
}

void QnStatusOverlayWidget::setGeometry(const QRectF &geometry) {
    QSizeF oldSize = size();

    base_type::setGeometry(geometry);

    if(!qFuzzyEquals(oldSize, size()))
        updateLayout();
}

QnStatusOverlayWidget::ButtonType QnStatusOverlayWidget::buttonType() const {
    return m_buttonType;
}

void QnStatusOverlayWidget::setButtonType(QnStatusOverlayWidget::ButtonType buttonType) {
    if (m_buttonType == buttonType)
        return;

    m_buttonType = buttonType;

    updateButtonText();
    updateButtonOpacity();
}

void QnStatusOverlayWidget::paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *) {
    if(m_statusOverlay == Qn::EmptyOverlay)
        return;

    if(m_pausedPainter.isNull()) {
        m_pausedPainter = qn_pausedPainterStorage()->get(QGLContext::currentContext());
        m_loadingProgressPainter = qn_loadingProgressPainterStorage()->get(QGLContext::currentContext());
    }

    QRectF rect = this->rect();

    if (m_statusOverlay == Qn::NoVideoDataOverlay)
    {
        auto color = palette().color(QPalette::Window);
        QRadialGradient gradient(rect.center(), rect.width()/2);
        gradient.setColorAt(0, shiftColor(color, 0x50, 0x50, 0x50));
        gradient.setColorAt(1, color);
        painter->fillRect(rect, gradient);
    }
    else {
        painter->fillRect(rect, palette().color(QPalette::Window));
    }

    if(m_statusOverlay == Qn::LoadingOverlay || m_statusOverlay == Qn::PausedOverlay || m_statusOverlay == Qn::EmptyOverlay) {
        qreal unit = qnGlobals->workbenchUnitSize();

        QnGlNativePainting::begin(QGLContext::currentContext(),painter);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        QRectF overlayRect(
            rect.center() - QPointF(unit / 10, unit / 10),
            QSizeF(unit / 5, unit / 5)
        );
        QMatrix4x4 m = QnOpenGLRendererManager::instance(QGLContext::currentContext())->pushModelViewMatrix();

        m.translate(overlayRect.center().x(), overlayRect.center().y(), 1.0);
        m.scale(overlayRect.width() / 2, overlayRect.height() / 2, 1.0);
        QnOpenGLRendererManager::instance(QGLContext::currentContext())->setModelViewMatrix(m);
        if(m_statusOverlay == Qn::LoadingOverlay) {
#ifdef QN_RESOURCE_WIDGET_FLASHY_LOADING_OVERLAY
            qint64 currentTimeMSec = QDateTime::currentMSecsSinceEpoch();
            m_loadingProgressPainter->paint(
                static_cast<qreal>(currentTimeMSec % defaultProgressPeriodMSec) / defaultProgressPeriodMSec,
                painter->opacity()
            );
#endif
        } else if(m_statusOverlay == Qn::PausedOverlay) {
            m_pausedPainter->paint(0.5 * painter->opacity());
        }
        QnOpenGLRendererManager::instance(QGLContext::currentContext())->popModelViewMatrix();

        glDisable(GL_BLEND);
        QnGlNativePainting::end(painter);

        if(m_statusOverlay == Qn::LoadingOverlay) {
#ifdef QN_RESOURCE_WIDGET_FLASHY_LOADING_OVERLAY
            paintFlashingText(painter, LoadingText, 0.125, QPointF(0.0, 0.15));
#else
            paintFlashingText(painter, LoadingText, 0.125);
#endif
        }
    }

    switch (m_statusOverlay) {
    case Qn::NoDataOverlay:
        paintFlashingText(painter, NoDataText, 0.125);
        break;
    case Qn::OfflineOverlay:
        paintFlashingText(painter, OfflineText, 0.125);
        break;
    case Qn::UnauthorizedOverlay:
        paintFlashingText(painter, UnauthorizedText, 0.125);
        paintFlashingText(painter, UnauthorizedSubText, 0.05, QPointF(0.0, 0.25));
        break;
    case Qn::ServerUnauthorizedOverlay:
        paintFlashingText(painter, UnauthorizedText, 0.125);
        break;
    case Qn::ServerOfflineOverlay:
        paintFlashingText(painter, ServerOfflineText, 0.125);
        break;
    case Qn::AnalogWithoutLicenseOverlay:
        {
            QRectF rect = this->rect();
            int count = qFloor(qMax(1.0, rect.height() / rect.width()) * 7.5);
            for (int i = -count; i <= count; i++)
                paintFlashingText(painter, AnalogLicenseText, 0.035, QPointF(0.0, 0.06 * i));
            break;
        }
    case Qn::VideowallWithoutLicenseOverlay:
        {
            QRectF rect = this->rect();
            int count = qFloor(qMax(1.0, rect.height() / rect.width()) * 7.5);
            for (int i = -count; i <= count; i++)
                paintFlashingText(painter, VideowallLicenseText, 0.035, QPointF(0.0, 0.06 * i));
            break;
        }
    case Qn::NoVideoDataOverlay:
        if (!m_ioSpeakerPixmap)
            m_ioSpeakerPixmap.reset(new QPixmap(qnSkin->pixmap("item/io_speaker.png")));
        paintPixmap(painter, *m_ioSpeakerPixmap, 0.064);
        break;
    case Qn::IoModuleDisabledOverlay:
        paintFlashingText(painter, IoModuleDisabledText, 0.112);
        break;
    default:
        break;
    }

}

void QnStatusOverlayWidget::paintFlashingText(QPainter *painter, OverlayText textId, qreal textSize, const QPointF &offset)
{
    QRect rect = this->rect().toRect();
    int unit = qMin(rect.width(), rect.height());

    QPixmap text = QnTextPixmapCache::instance()->pixmap(
        m_texts[textId],
        m_staticFont,
        palette().color(QPalette::WindowText));

    qreal scaleFactor = textSize * unit / kStaticFontSize;
    const auto dpTextSize = text.size() / text.devicePixelRatio();
    if (dpTextSize.width() * scaleFactor > rect.width())
        scaleFactor = rect.width() / dpTextSize.width();

    QnScopedPainterTransformRollback transformRollback(painter);

    qreal opacity = painter->opacity();
    painter->setOpacity(opacity * qAbs(std::sin(QDateTime::currentMSecsSinceEpoch() / qreal(kFlashingPeriodMs * 2) * M_PI)));
    painter->translate(rect.center() + offset * unit);
    painter->scale(scaleFactor, scaleFactor);
    paintPixmapSharp(painter, text, -toPoint(dpTextSize / 2));
    painter->setOpacity(opacity);
}

void QnStatusOverlayWidget::paintPixmap(QPainter *painter, const QPixmap &picture, qreal imageSize)
{
    QRectF rect = this->rect();
    qreal unit = qMin(rect.width(), rect.height());

    QnScopedPainterTransformRollback transformRollback(painter);

    qreal scaleFactor = imageSize * unit / kStaticFontSize;
    const auto dpPictureSize = picture.size() / picture.devicePixelRatio();
    if (dpPictureSize.width() * scaleFactor > rect.width())
        scaleFactor = rect.width() / dpPictureSize.width();

    qreal opacity = painter->opacity();
    painter->translate(rect.center());
    painter->scale(scaleFactor, scaleFactor);

    painter->drawPixmap(-toPoint(dpPictureSize / 2), picture);
    painter->setOpacity(opacity);
}
