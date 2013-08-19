#ifndef QN_RESOURCE_STATUS_OVERLAY_WIDGET_H
#define QN_RESOURCE_STATUS_OVERLAY_WIDGET_H

#include <QtCore/QSharedPointer>
#include <QtGui/QStaticText>

#include <client/client_globals.h>

#include <ui/graphics/items/standard/graphics_widget.h>
#include <ui/common/geometry.h>

class QnPausedPainter;
class QnLoadingProgressPainter;
class QnTextButtonWidget;

class QnStatusOverlayWidget: public GraphicsWidget, protected QnGeometry {
    Q_OBJECT
    typedef GraphicsWidget base_type;

public:
    QnStatusOverlayWidget(QGraphicsWidget *parent = NULL, Qt::WindowFlags windowFlags = 0);
    virtual ~QnStatusOverlayWidget();

    Qn::ResourceStatusOverlay statusOverlay() const;
    void setStatusOverlay(Qn::ResourceStatusOverlay statusOverlay);

    virtual void setGeometry(const QRectF &geometry) override;

    bool isDiagnosticsVisible() const;
    void setDiagnosticsVisible(bool diagnosticsVisible);

signals:
    void statusOverlayChanged();
    void diagnosticsRequested();

protected:
    virtual void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

private:
    void paintFlashingText(QPainter *painter, const QStaticText &text, qreal textSize, const QPointF &offset = QPointF());

    void updateLayout();
    void updateDiagnosticsButtonOpacity(bool animate = true);

private:
    QSharedPointer<QnPausedPainter> m_pausedPainter;
    QSharedPointer<QnLoadingProgressPainter> m_loadingProgressPainter;

    // TODO: #Elric share these.
    QStaticText m_noDataStaticText;
    QStaticText m_offlineStaticText;
    QStaticText m_unauthorizedStaticText;
    QStaticText m_unauthorizedStaticSubText;
    QStaticText m_loadingStaticText;
    QStaticText m_analogLicenseStaticText;

    Qn::ResourceStatusOverlay m_statusOverlay;

    bool m_diagnosticsVisible;
    QnTextButtonWidget *m_diagnosticsButton;
};

#endif // QN_RESOURCE_STATUS_OVERLAY_WIDGET_H
