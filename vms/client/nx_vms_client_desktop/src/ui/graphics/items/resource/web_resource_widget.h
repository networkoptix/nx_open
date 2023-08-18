// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/network/ssl/helpers.h>
#include <ui/graphics/items/resource/resource_widget.h>

namespace nx::vms::client::desktop { class GraphicsWebEngineView; }

class QnWebResourceWidget: public QnResourceWidget
{
    Q_OBJECT
    using base_type = QnResourceWidget;

public:
    QnWebResourceWidget(
        nx::vms::client::desktop::SystemContext* systemContext,
        nx::vms::client::desktop::WindowContext* windowContext,
        QnWorkbenchItem* item,
        QGraphicsItem *parent = nullptr);
    virtual ~QnWebResourceWidget();

    virtual bool eventFilter(QObject* object, QEvent* event) override;

    nx::vms::client::desktop::GraphicsWebEngineView* webView() const;

    void setMinimalTitleBarMode(bool value);

    bool overlayIsVisible() const;
    void setOverlayVisibility(bool visible);

    void setPreventDefaultContextMenu(bool value);

protected:
    virtual int helpTopicAt(const QPointF& pos) const override;

    virtual QString calculateDetailsText() const override;
    virtual QPixmap calculateDetailsIcon() const override;
    virtual Qn::ResourceStatusOverlay calculateStatusOverlay() const override;
    virtual int calculateButtonsVisibility() const override;
    virtual QString calculateTitleText() const override;

    virtual Qn::RenderStatus paintChannelBackground(
        QPainter *painter, int channel, const QRectF &channelRect, const QRectF &paintRect) override;

private:
    void initClientApiSupport();
    void setupOverlays();
    void setupWidget();
    bool verifyCertificate(const QString& pemString, const QUrl& url);
    bool askUserToAcceptCertificate(
        const nx::network::ssl::CertificateChain& chain, const nx::utils::Url& url);

private:
    std::unique_ptr<nx::vms::client::desktop::GraphicsWebEngineView> m_webEngineView;
    bool m_validCertificate = true;
    bool m_pageLoaded = false;
    bool m_isMinimalTitleBar = false;
    bool m_overlayIsVisible = true;
};
