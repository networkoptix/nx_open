#ifndef QN_RESOURCE_STATUS_OVERLAY_WIDGET_H
#define QN_RESOURCE_STATUS_OVERLAY_WIDGET_H

#include <QtCore/QSharedPointer>
#include <QtGui/QStaticText>

#include <client/client_globals.h>

#include <core/resource/resource_fwd.h>

#include <ui/graphics/items/standard/graphics_widget.h>
#include <ui/common/geometry.h>

#include <memory>
#include <array>

class QnPausedPainter;
class QnLoadingProgressPainter;
class QnTextButtonWidget;

class QnStatusOverlayWidget: public GraphicsWidget, protected QnGeometry {
    Q_OBJECT
    typedef GraphicsWidget base_type;

public:
    enum ButtonType {
        NoButton,
        DiagnosticsButton,
        IoEnableButton,
        MoreLicensesButton
    };

    QnStatusOverlayWidget(const QnResourcePtr &resource,  QGraphicsWidget *parent = NULL, Qt::WindowFlags windowFlags = 0);
    virtual ~QnStatusOverlayWidget();

    Qn::ResourceStatusOverlay statusOverlay() const;
    void setStatusOverlay(Qn::ResourceStatusOverlay statusOverlay);

    virtual void setGeometry(const QRectF &geometry) override;

    ButtonType buttonType() const;
    void setButtonType(ButtonType buttonType);

signals:
    void statusOverlayChanged();

    void diagnosticsRequested();
    void ioEnableRequested();
    void moreLicensesRequested();

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
        AnalogLicenseText,
        VideowallLicenseText,
        IoModuleDisabledText,
        TextCount
    };

    void paintFlashingText(QPainter *painter, OverlayText textId, qreal textSize, const QPointF &offset = QPointF());
    void paintPixmap(QPainter *painter, const QPixmap &picture, qreal imageSize);

    void updateLayout();
    void updateButtonOpacity(bool animate = true);
    void updateButtonText();

private:
    void setButtonVisible(bool visible
        , bool animate);

private:
    const QnResourcePtr m_resource;
    QSharedPointer<QnPausedPainter> m_pausedPainter;
    QSharedPointer<QnLoadingProgressPainter> m_loadingProgressPainter;
    Qn::ResourceStatusOverlay m_statusOverlay;

    std::array<QString, TextCount> m_texts;

    QFont m_staticFont;

    ButtonType m_buttonType;
    QnTextButtonWidget *m_button;
    std::unique_ptr<QPixmap> m_ioSpeakerPixmap;
};

#endif // QN_RESOURCE_STATUS_OVERLAY_WIDGET_H
