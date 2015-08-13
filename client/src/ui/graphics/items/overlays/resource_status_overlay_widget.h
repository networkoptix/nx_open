#ifndef QN_RESOURCE_STATUS_OVERLAY_WIDGET_H
#define QN_RESOURCE_STATUS_OVERLAY_WIDGET_H

#include <QtCore/QSharedPointer>
#include <QtGui/QStaticText>

#include <client/client_globals.h>

#include <ui/graphics/items/standard/graphics_widget.h>
#include <ui/common/geometry.h>
#include <memory>

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

    bool isIoEnableButtonVisible() const;
    void setEnableButtonVisible(bool visible);

signals:
    void statusOverlayChanged();
    void diagnosticsRequested();
    void ioEnableRequested();

protected:
    virtual void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

private:
    enum OverlayText {
        NoDataText,
        OfflineText,
        ServerOfflineText,
        UnauthorizedText,
        UnauthorizedSubText,
        LoadingText,
        NoVideoStreamText,
        AnalogLicenseText,
        VideowallLicenseText,
        IoModuleDisabledText,
        IoModuleLicenseRequiredText,
        TextCount
    };

    void paintFlashingText(QPainter *painter, const QStaticText &text, qreal textSize, const QPointF &offset = QPointF());
    void paintPixmap(QPainter *painter, const QPixmap &picture, qreal imageSize);

    void updateLayout();
    void updateButtonsOpacity(bool animate = true);

private:
    QSharedPointer<QnPausedPainter> m_pausedPainter;
    QSharedPointer<QnLoadingProgressPainter> m_loadingProgressPainter;
    Qn::ResourceStatusOverlay m_statusOverlay;

    boost::array<QStaticText, TextCount> m_staticTexts;
    QFont m_staticFont;

    bool m_diagnosticsVisible;
    QnTextButtonWidget *m_diagnosticsButton;
    bool m_ioEnableButtonVisible;
    QnTextButtonWidget *m_ioEnableButton;
    std::unique_ptr<QPixmap> m_ioSpeakerPixmap;
};

#endif // QN_RESOURCE_STATUS_OVERLAY_WIDGET_H
