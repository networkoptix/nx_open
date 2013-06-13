#include "resource_status_overlay_widget.h"

#include <cmath> /* For std::sin. */

#include <utils/common/scoped_painter_rollback.h>

#include <ui/graphics/painters/loading_progress_painter.h>
#include <ui/graphics/painters/paused_painter.h>
#include <ui/graphics/opengl/gl_context_data.h>

#include <ui/style/globals.h>


/** @def QN_RESOURCE_WIDGET_FLASHY_LOADING_OVERLAY
 * 
 * When defined, makes loading overlay much more 'flashy', drawing loading circles
 * and reducing timeout after which overlay is drawn.
 */
// #define QN_RESOURCE_WIDGET_FLASHY_LOADING_OVERLAY

namespace {

    /** Flashing text flash interval. */
    static const int testFlashingPeriodMSec = 1000;

    class QnLoadingProgressPainterFactory {
    public:
        QnLoadingProgressPainter *operator()(const QGLContext *context) {
            return new QnLoadingProgressPainter(0.5, 12, 0.5, QColor(255, 255, 255, 0), QColor(255, 255, 255, 255), context);
        }
    };

    typedef QnGlContextData<QnLoadingProgressPainter, QnLoadingProgressPainterFactory> QnLoadingProgressPainterStorage;
    Q_GLOBAL_STATIC(QnLoadingProgressPainterStorage, qn_loadingProgressPainterStorage);

    Q_GLOBAL_STATIC(QnGlContextData<QnPausedPainter>, qn_pausedPainterStorage);

} // anonymous namespace


QnStatusOverlayWidget::QnStatusOverlayWidget(QGraphicsWidget *parent, Qt::WindowFlags windowFlags):
    base_type(parent, windowFlags),
    m_statusOverlay(Qn::EmptyOverlay)
{
    setAcceptedMouseButtons(0);

    /* Init static text. */
    m_noDataStaticText.setText(tr("NO DATA"));
    m_noDataStaticText.setPerformanceHint(QStaticText::AggressiveCaching);
    m_offlineStaticText.setText(tr("NO SIGNAL"));
    m_offlineStaticText.setPerformanceHint(QStaticText::AggressiveCaching);
    m_unauthorizedStaticText.setText(tr("Unauthorized"));
    m_unauthorizedStaticText.setPerformanceHint(QStaticText::AggressiveCaching);
    m_unauthorizedStaticSubText.setText(tr("Please check authentication information in camera settings"));
    m_unauthorizedStaticSubText.setPerformanceHint(QStaticText::AggressiveCaching);
    m_loadingStaticText.setText(tr("Loading..."));
    m_loadingStaticText.setPerformanceHint(QStaticText::AggressiveCaching);
    m_analogLicenseStaticText.setText(tr("Activate analog license to remove this message"));
    m_analogLicenseStaticText.setPerformanceHint(QStaticText::AggressiveCaching);
}

QnStatusOverlayWidget::~QnStatusOverlayWidget() {
    return;
}

Qn::ResourceStatusOverlay QnStatusOverlayWidget::statusOverlay() const {
    return m_statusOverlay;
}

void QnStatusOverlayWidget::setStatusOverlay(Qn::ResourceStatusOverlay statusOverlay) {
    m_statusOverlay = statusOverlay;
}

void QnStatusOverlayWidget::paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *) {
    if(m_statusOverlay == Qn::EmptyOverlay)
        return;

    if(m_pausedPainter.isNull()) {
        m_pausedPainter = qn_pausedPainterStorage()->get(QGLContext::currentContext());
        m_loadingProgressPainter = qn_loadingProgressPainterStorage()->get(QGLContext::currentContext());
    }

    QRectF rect = this->rect();

    painter->fillRect(rect, QColor(0, 0, 0, 128));

    if(m_statusOverlay == Qn::LoadingOverlay || m_statusOverlay == Qn::PausedOverlay || m_statusOverlay == Qn::EmptyOverlay) {
        qreal unit = qnGlobals->workbenchUnitSize();

        painter->beginNativePainting();
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        QRectF overlayRect(
            rect.center() - QPointF(unit / 10, unit / 10),
            QSizeF(unit / 5, unit / 5)
        );

        glPushMatrix();
        glTranslatef(overlayRect.center().x(), overlayRect.center().y(), 1.0);
        glScalef(overlayRect.width() / 2, overlayRect.height() / 2, 1.0);
        //glRotatef(-1.0 * m_overlayRotation, 0.0, 0.0, 1.0);
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
        glPopMatrix();

        glDisable(GL_BLEND);
        painter->endNativePainting();

        if(m_statusOverlay == Qn::LoadingOverlay) {
#ifdef QN_RESOURCE_WIDGET_FLASHY_LOADING_OVERLAY
            paintFlashingText(painter, m_loadingStaticText, 0.05, QPointF(0.0, 0.15));
#else
            paintFlashingText(painter, m_loadingStaticText, 0.05);
#endif
        }
    }

    if (m_statusOverlay == Qn::NoDataOverlay) {
        paintFlashingText(painter, m_noDataStaticText, 0.1);
    } else if (m_statusOverlay == Qn::OfflineOverlay) {
        paintFlashingText(painter, m_offlineStaticText, 0.1);
    } else if (m_statusOverlay == Qn::UnauthorizedOverlay) {
        paintFlashingText(painter, m_unauthorizedStaticText, 0.1);
        paintFlashingText(painter, m_unauthorizedStaticSubText, 0.025, QPointF(0.0, 0.1));
    } else if (m_statusOverlay == Qn::AnalogWithoutLicenseOverlay) {
        for (int i = -5; i < 6; i++)
            paintFlashingText(painter, m_analogLicenseStaticText, 0.025, QPointF(0.0, 0.05*i));
    }
}

void QnStatusOverlayWidget::paintFlashingText(QPainter *painter, const QStaticText &text, qreal textSize, const QPointF &offset) {
    qreal unit = qnGlobals->workbenchUnitSize(); //channelRect(0).width();

    QFont font;
    font.setPointSizeF(textSize * unit);
    font.setStyleHint(QFont::SansSerif, QFont::ForceOutline);

    QnScopedPainterFontRollback fontRollback(painter, font);
    QnScopedPainterPenRollback penRollback(painter, QPen(QColor(255, 208, 208, 196)));
    QnScopedPainterTransformRollback transformRollback(painter);

    qreal opacity = painter->opacity();
    painter->setOpacity(opacity * qAbs(std::sin(QDateTime::currentMSecsSinceEpoch() / qreal(testFlashingPeriodMSec * 2) * M_PI)));

    painter->translate(rect().center());
    /*painter->rotate(-1.0 * m_overlayRotation);
    painter->translate(offset * unit);
    if (m_overlayRotation % 180 != 0) {
        qreal ratio = 1 / ( m_aspectRatio > 0.0 ? m_aspectRatio : m_enclosingAspectRatio);
        painter->scale(ratio, ratio);
    }*/

    painter->drawStaticText(-toPoint(text.size() / 2), text);
    painter->setOpacity(opacity);

    Q_UNUSED(transformRollback)
    Q_UNUSED(penRollback)
    Q_UNUSED(fontRollback)
}