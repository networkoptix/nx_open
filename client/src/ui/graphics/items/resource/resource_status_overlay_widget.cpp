#include "resource_status_overlay_widget.h"

#include <cmath> /* For std::sin. */

#include <QDateTime>

#include <utils/common/scoped_painter_rollback.h>

#include <ui/graphics/items/generic/image_button_widget.h>
#include <ui/graphics/painters/loading_progress_painter.h>
#include <ui/graphics/painters/paused_painter.h>
#include <ui/graphics/opengl/gl_context_data.h>

#include <ui/animation/opacity_animator.h>
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

    const QColor textColor(255, 96, 96, 128);
    const QColor buttonBaseColor(255, 32, 32, 160);
    const QColor buttonBorderColor(255, 32, 32, 255);

} // anonymous namespace


QnStatusOverlayWidget::QnStatusOverlayWidget(QGraphicsWidget *parent, Qt::WindowFlags windowFlags):
    base_type(parent, windowFlags),
    m_statusOverlay(Qn::EmptyOverlay),
    m_diagnosticsVisible(false)
{
    setAcceptedMouseButtons(0);

    /* Init static text. */
    m_noDataStaticText.setText(tr("NO DATA"));
    m_noDataStaticText.setPerformanceHint(QStaticText::AggressiveCaching);
    m_offlineStaticText.setText(tr("NO SIGNAL"));
    m_offlineStaticText.setPerformanceHint(QStaticText::AggressiveCaching);
    m_unauthorizedStaticText.setText(tr("Unauthorized"));
    m_unauthorizedStaticText.setPerformanceHint(QStaticText::AggressiveCaching);
    m_unauthorizedStaticSubText.setText(tr("Please check authentication information<br/>in camera settings"));
    m_unauthorizedStaticSubText.setPerformanceHint(QStaticText::AggressiveCaching);
    m_unauthorizedStaticSubText.setTextOption(QTextOption(Qt::AlignCenter));
    m_loadingStaticText.setText(tr("Loading..."));
    m_loadingStaticText.setPerformanceHint(QStaticText::AggressiveCaching);
    m_analogLicenseStaticText.setText(tr("Activate analog license to remove this message"));
    m_analogLicenseStaticText.setPerformanceHint(QStaticText::AggressiveCaching);

    /* Init buttons. */
    m_diagnosticsButton = new QnTextButtonWidget(this);
    m_diagnosticsButton->setText(tr("Diagnose..."));
    m_diagnosticsButton->setFrameShape(Qn::RectangularFrame);
    m_diagnosticsButton->setRelativeFontSize(0.5);
    m_diagnosticsButton->setRelativeFrameWidth(1.0 / 16.0);
    m_diagnosticsButton->setStateOpacity(0, 0.4);
    m_diagnosticsButton->setStateOpacity(QnImageButtonWidget::HOVERED, 0.7);
    m_diagnosticsButton->setStateOpacity(QnImageButtonWidget::PRESSED, 1.0);
    m_diagnosticsButton->setWindowColor(buttonBaseColor);
    m_diagnosticsButton->setFrameColor(buttonBorderColor);

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

    if(!qFuzzyCompare(oldSize, size()))
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
            paintFlashingText(painter, m_loadingStaticText, 0.125, QPointF(0.0, 0.15));
#else
            paintFlashingText(painter, m_loadingStaticText, 0.125);
#endif
        }
    }

    if (m_statusOverlay == Qn::NoDataOverlay) {
        paintFlashingText(painter, m_noDataStaticText, 0.125);
    } else if (m_statusOverlay == Qn::OfflineOverlay) {
        paintFlashingText(painter, m_offlineStaticText, 0.125);
    } else if (m_statusOverlay == Qn::UnauthorizedOverlay) {
        paintFlashingText(painter, m_unauthorizedStaticText, 0.125);
        paintFlashingText(painter, m_unauthorizedStaticSubText, 0.05, QPointF(0.0, 0.25));
    } else if (m_statusOverlay == Qn::AnalogWithoutLicenseOverlay) {
        QRectF rect = this->rect();
        int count = qFloor(qMax(1.0, rect.height() / rect.width()) * 7.5);
        for (int i = -count; i <= count; i++)
            paintFlashingText(painter, m_analogLicenseStaticText, 0.035, QPointF(0.0, 0.06 * i));
    }
}

void QnStatusOverlayWidget::paintFlashingText(QPainter *painter, const QStaticText &text, qreal textSize, const QPointF &offset) {
    QRectF rect = this->rect();
    qreal unit = qMin(rect.width(), rect.height());

    QFont font;
    font.setPointSizeF(textSize * 1000);
    font.setStyleHint(QFont::SansSerif, QFont::ForceOutline);

    QnScopedPainterFontRollback fontRollback(painter, font);
    QnScopedPainterPenRollback penRollback(painter, textColor);
    QnScopedPainterTransformRollback transformRollback(painter);

    qreal opacity = painter->opacity();
    painter->setOpacity(opacity * qAbs(std::sin(QDateTime::currentMSecsSinceEpoch() / qreal(testFlashingPeriodMSec * 2) * M_PI)));
    painter->translate(rect.center() + offset * unit);
    painter->scale(unit / 1000, unit / 1000);

    painter->drawStaticText(-toPoint(text.size() / 2), text);
    painter->setOpacity(opacity);

    Q_UNUSED(transformRollback)
    Q_UNUSED(penRollback)
    Q_UNUSED(fontRollback)
}
