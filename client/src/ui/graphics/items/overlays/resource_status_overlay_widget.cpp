#include "resource_status_overlay_widget.h"

#include <cmath> /* For std::sin. */

#include <QtCore/QDateTime>
#include <QtCore/QtMath>

#include <utils/common/scoped_painter_rollback.h>

#include <ui/animation/opacity_animator.h>
#include <ui/graphics/items/generic/text_button_widget.h>
#include <ui/graphics/painters/loading_progress_painter.h>
#include <ui/graphics/painters/paused_painter.h>
#include <ui/graphics/opengl/gl_context_data.h>
#include <ui/style/globals.h>
#include <ui/workaround/gl_native_painting.h>
#include "opengl_renderer.h"

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

    const qreal staticFontSize = 50;

    const int flashingPeriodMSec = 1000;

} // anonymous namespace


QnStatusOverlayWidget::QnStatusOverlayWidget(QGraphicsWidget *parent, Qt::WindowFlags windowFlags):
    base_type(parent, windowFlags),
    m_statusOverlay(Qn::EmptyOverlay),
    m_diagnosticsVisible(false)
{
    setAcceptedMouseButtons(0);

    /* Init static text. */
    m_staticFont.setPointSizeF(staticFontSize);
    m_staticFont.setStyleHint(QFont::SansSerif, QFont::ForceOutline);

    m_staticTexts[NoDataText] = tr("NO DATA");
    m_staticTexts[OfflineText] = tr("NO SIGNAL");
    m_staticTexts[ServerOfflineText] = tr("Server offline");
    m_staticTexts[UnauthorizedText] = tr("Unauthorized");
    m_staticTexts[UnauthorizedSubText] = tr("Please check authentication information<br/>in camera settings");
    m_staticTexts[AnalogLicenseText] = tr("Activate analog license to remove this message");
    m_staticTexts[VideowallLicenseText] = tr("Activate Video Wall license to remove this message");
    m_staticTexts[LoadingText] = tr("Loading...");

    for(int i = 0; i < m_staticTexts.size(); i++) {
        m_staticTexts[i].setPerformanceHint(QStaticText::AggressiveCaching);
        m_staticTexts[i].setTextOption(QTextOption(Qt::AlignCenter));
        m_staticTexts[i].prepare(QTransform(), m_staticFont);
    }

    /* Init buttons. */
    m_diagnosticsButton = new QnTextButtonWidget(this);
    m_diagnosticsButton->setObjectName(lit("diagnosticsButton"));
    m_diagnosticsButton->setText(tr("Diagnose..."));
    m_diagnosticsButton->setFrameShape(Qn::RectangularFrame);
    m_diagnosticsButton->setRelativeFrameWidth(1.0 / 16.0);
    m_diagnosticsButton->setStateOpacity(0, 0.4);
    m_diagnosticsButton->setStateOpacity(QnImageButtonWidget::Hovered, 0.7);
    m_diagnosticsButton->setStateOpacity(QnImageButtonWidget::Pressed, 1.0);
    m_diagnosticsButton->setFont(m_staticFont);
    //m_diagnosticsButton->setVisible(false);

    connect(m_diagnosticsButton, SIGNAL(clicked()), this, SIGNAL(diagnosticsRequested()));

    updateLayout();
    updateDiagnosticsButtonOpacity(false);
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

    updateDiagnosticsButtonOpacity();

    emit statusOverlayChanged();
}

bool QnStatusOverlayWidget::isDiagnosticsVisible() const {
    return m_diagnosticsVisible;
}

void QnStatusOverlayWidget::setDiagnosticsVisible(bool diagnosticsVisible) {
    if(m_diagnosticsVisible == diagnosticsVisible)
        return;

    m_diagnosticsVisible = diagnosticsVisible;

    updateDiagnosticsButtonOpacity(false);
}

void QnStatusOverlayWidget::updateLayout() {
    QRectF rect = this->rect();

    qreal point = qMin(rect.width(), rect.height()) / 16.0;
    QSizeF size(point * 6.0, point * 1.5);

    m_diagnosticsButton->setGeometry(QRectF(rect.center() - toPoint(size) / 2.0 + QPointF(0.0, point * 4.0), size));
}

void QnStatusOverlayWidget::updateDiagnosticsButtonOpacity(bool animate) {
    qreal opacity = (m_diagnosticsVisible && m_statusOverlay == Qn::OfflineOverlay) ? 1.0 : 0.0;

    if(animate) {
        opacityAnimator(m_diagnosticsButton)->animateTo(opacity);
    } else {
        m_diagnosticsButton->setOpacity(opacity);
    }
}

void QnStatusOverlayWidget::setGeometry(const QRectF &geometry) {
    QSizeF oldSize = size();

    base_type::setGeometry(geometry);

    if(!qFuzzyEquals(oldSize, size()))
        updateLayout();
}

void QnStatusOverlayWidget::paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *) {
    if(m_statusOverlay == Qn::EmptyOverlay)
        return;

    if(m_pausedPainter.isNull()) {
        m_pausedPainter = qn_pausedPainterStorage()->get(QGLContext::currentContext());
        m_loadingProgressPainter = qn_loadingProgressPainterStorage()->get(QGLContext::currentContext());
    }

    QRectF rect = this->rect();

    painter->fillRect(rect, palette().color(QPalette::Window));

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
            paintFlashingText(painter, m_staticTexts[LoadingText], 0.125, QPointF(0.0, 0.15));
#else
            paintFlashingText(painter, m_staticTexts[LoadingText], 0.125);
#endif
        }
    }

    switch (m_statusOverlay) {
    case Qn::NoDataOverlay:
        paintFlashingText(painter, m_staticTexts[NoDataText], 0.125);
        break;
    case Qn::OfflineOverlay:
        paintFlashingText(painter, m_staticTexts[OfflineText], 0.125);
        break;
    case Qn::UnauthorizedOverlay:
        paintFlashingText(painter, m_staticTexts[UnauthorizedText], 0.125);
        paintFlashingText(painter, m_staticTexts[UnauthorizedSubText], 0.05, QPointF(0.0, 0.25));
        break;
    case Qn::ServerOfflineOverlay:
        paintFlashingText(painter, m_staticTexts[ServerOfflineText], 0.125);
        break;
    case Qn::AnalogWithoutLicenseOverlay: 
        {
            QRectF rect = this->rect();
            int count = qFloor(qMax(1.0, rect.height() / rect.width()) * 7.5);
            for (int i = -count; i <= count; i++)
                paintFlashingText(painter, m_staticTexts[AnalogLicenseText], 0.035, QPointF(0.0, 0.06 * i));
            break;
        }   
    case Qn::VideowallWithoutLicenseOverlay:
        {
            QRectF rect = this->rect();
            int count = qFloor(qMax(1.0, rect.height() / rect.width()) * 7.5);
            for (int i = -count; i <= count; i++)
                paintFlashingText(painter, m_staticTexts[VideowallLicenseText], 0.035, QPointF(0.0, 0.06 * i));
            break;
        }
    default:
        break;
    }

}

void QnStatusOverlayWidget::paintFlashingText(QPainter *painter, const QStaticText &text, qreal textSize, const QPointF &offset) {
    QRectF rect = this->rect();
    qreal unit = qMin(rect.width(), rect.height());

    QnScopedPainterFontRollback fontRollback(painter, m_staticFont);
    QnScopedPainterPenRollback penRollback(painter, palette().color(QPalette::WindowText));
    QnScopedPainterTransformRollback transformRollback(painter);

    qreal opacity = painter->opacity();
    painter->setOpacity(opacity * qAbs(std::sin(QDateTime::currentMSecsSinceEpoch() / qreal(flashingPeriodMSec * 2) * M_PI)));
    painter->translate(rect.center() + offset * unit);
    painter->scale(textSize * unit / staticFontSize, textSize * unit / staticFontSize);

    painter->drawStaticText(-toPoint(text.size() / 2), text);
    painter->setOpacity(opacity);

    Q_UNUSED(transformRollback)
    Q_UNUSED(penRollback)
    Q_UNUSED(fontRollback) // TODO: #Elric remove
}
