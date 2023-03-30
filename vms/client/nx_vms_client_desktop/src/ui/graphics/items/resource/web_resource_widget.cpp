// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "web_resource_widget.h"

#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>

#include <core/resource/webpage_resource.h>
#include <nx/network/ssl/certificate.h>
#include <nx/utils/string.h>
#include <nx/utils/url.h>
#include <nx/vms/client/desktop/common/widgets/webview_controller.h>
#include <nx/vms/client/desktop/integrations/c2p/jsapi/external.h>
#include <nx/vms/client/desktop/jsapi/globals.h>
#include <nx/vms/client/desktop/jsapi/logger.h>
#include <nx/vms/client/desktop/jsapi/resources.h>
#include <nx/vms/client/desktop/jsapi/tab.h>
#include <nx/vms/client/desktop/style/skin.h>
#include <nx/vms/client/desktop/ui/actions/action_manager.h>
#include <nx/vms/common/html/html.h>
#include <ui/dialogs/common/session_aware_dialog.h>
#include <ui/graphics/items/generic/image_button_bar.h>
#include <ui/graphics/items/generic/image_button_widget.h>
#include <ui/graphics/items/overlays/resource_title_item.h>
#include <ui/graphics/items/resource/button_ids.h>
#include <ui/graphics/items/standard/graphics_web_view.h>
#include <ui/help/help_topics.h>
#include <ui/workbench/workbench.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_display.h>
#include <ui/workbench/workbench_item.h>

using namespace nx::vms::client::desktop;

QnWebResourceWidget::QnWebResourceWidget(
    QnWorkbenchContext* context,
    QnWorkbenchItem* item,
    QGraphicsItem* parent)
    :
    base_type(context, item, parent),
    m_webEngineView(item->data(Qn::ItemWebPageSavedStateDataRole).isNull()
        ? new GraphicsWebEngineView(resource(), this)
        : new GraphicsWebEngineView({}, this)) //< Page will be loaded from saved state later.
{
    setOption(AllowFocus, false);

    m_webEngineView->installEventFilter(this);
    m_webEngineView->controller()->setCertificateValidator(
        [this](const QString& pemString, const QUrl& url)
        {
            bool result = verifyCertificate(pemString, url);
            if (!result)
            {
                // Stay on current page, if it was loaded, close widget otherwise.
                if (m_pageLoaded)
                    m_webEngineView->controller()->stop();
                else
                    close();
            }
            return result;
        });

    const auto contentMargins = QMarginsF(0, 36, 0, 0);
    const auto webParams = detail::OverlayParams(Visible, OverlayFlag::bindToViewport, BaseLayer,
        contentMargins);

    addOverlayWidget(m_webEngineView.get(), webParams);
    setFocusProxy(m_webEngineView.get());
    connect(this, &QGraphicsWidget::geometryChanged,
        m_webEngineView, &GraphicsQmlView::updateWindowGeometry);

    setOption(QnResourceWidget::WindowRotationForbidden, true);
    updateTitleText();
    updateInfoText();
    updateDetailsText();

    const auto updateStatusesHandler =
        [this]()
        {
            const auto status = m_webEngineView->status();
            const auto resourceStatus = (status == kPageLoadFailed)
                ? nx::vms::api::ResourceStatus::offline
                : nx::vms::api::ResourceStatus::online;

            resource()->setStatus(resourceStatus);
            updateStatusOverlay(true);
        };

    connect(m_webEngineView, &GraphicsWebEngineView::statusChanged, this, updateStatusesHandler);
    connect(m_webEngineView, &GraphicsWebEngineView::loadFinished, this,
        [this](bool ok)
        {
            if (ok)
                m_pageLoaded = true;
        });

    setupOverlays();
    updateButtonsVisibility();

    initClientApiSupport();
}

void QnWebResourceWidget::initClientApiSupport()
{
    m_webEngineView->controller()->registerApiObjectWithFactory("external",
        [this](QObject* parent) -> QObject*
        {
            using External = integrations::c2p::jsapi::External;
            return new External(workbench()->context(), this->item(), parent);
        });

    m_webEngineView->controller()->registerApiObjectWithFactory("vms.tab",
        [this](QObject* parent) -> QObject*
        {
            return new jsapi::Tab(workbench()->context(), parent);
        });

    m_webEngineView->controller()->registerApiObjectWithFactory("vms.resources",
        [this](QObject* parent) -> QObject*
        {
            return new jsapi::Resources(workbench()->context(), parent);
        });

    m_webEngineView->controller()->registerApiObjectWithFactory("vms.log",
        [](QObject* parent) -> QObject*
        {
            return new jsapi::Logger(parent);
        });

    m_webEngineView->controller()->registerApiObjectWithFactory("vms",
        [](QObject* parent) -> QObject*
        {
            return new jsapi::Globals(parent);
        });
}

QnWebResourceWidget::~QnWebResourceWidget()
{
    /* statusChanged will be emitted during the destruction process. */
    m_webEngineView->disconnect(this);
}

int QnWebResourceWidget::helpTopicAt(const QPointF& /*pos*/) const
{
    return Qn::MainWindow_WebPageItem_Help;
}

void QnWebResourceWidget::setupOverlays()
{
    {
        // Left buttons bar setup
        auto buttonsBar = titleBar()->leftButtonsBar();

        auto backButton = createStatisticAwareButton(lit("web_widget_back"));
        backButton->setIcon(loadSvgIcon("item/back.svg"));
        backButton->setObjectName("BackButton");
        connect(backButton, &QnImageButtonWidget::clicked, m_webEngineView, &GraphicsWebEngineView::back);
        buttonsBar->addButton(Qn::BackButton, backButton);
        buttonsBar->setButtonsEnabled(Qn::BackButton, false);

        connect(m_webEngineView, &GraphicsWebEngineView::canGoBackChanged, this,
            [this, buttonsBar]()
            {
                buttonsBar->setButtonsEnabled(Qn::BackButton, m_webEngineView->canGoBack());
            });

        // Should force HUD to update details text with new URL.
        connect(m_webEngineView, &GraphicsWebEngineView::loadStarted, this,
            [this]()
            {
                this->updateDetailsText();
            });

        auto reloadButton = createStatisticAwareButton(lit("web_widget_reload"));
        reloadButton->setIcon(loadSvgIcon("item/refresh.svg"));
        reloadButton->setObjectName("ReloadButton");
        connect(reloadButton, &QnImageButtonWidget::clicked,
            m_webEngineView, &GraphicsWebEngineView::reload);

        buttonsBar->addButton(Qn::ReloadPageButton, reloadButton);
    }
}

Qn::ResourceStatusOverlay QnWebResourceWidget::calculateStatusOverlay() const
{
    switch (m_webEngineView->status())
    {
    case kPageInitialLoadInProgress:
        return Qn::LoadingOverlay;
    //case kPageLoading:
    //case kPageLoaded:
    //case kPageLoadFailed:
    default:
        return Qn::EmptyOverlay;
    }
}

bool QnWebResourceWidget::eventFilter(QObject* object, QEvent* event)
{
    if (object == m_webEngineView.get())
    {
        const auto isFocused =
            [this]() { return display() && display()->view() && display()->view()->hasFocus(); };

        if (event->type() == QEvent::GraphicsSceneMousePress)
        {
            auto mouseEvent = static_cast<QGraphicsSceneMouseEvent*>(event);
            mousePressEvent(mouseEvent);
            if (mouseEvent->button() == Qt::RightButton)
                return true;
        }
        if (event->type() == QEvent::GraphicsSceneContextMenu)
            return true;
        if ((event->type() == QEvent::KeyPress || event->type() == QEvent::KeyRelease) && !isFocused())
            return true;
    }

    return base_type::eventFilter(object, event);
}

int QnWebResourceWidget::calculateButtonsVisibility() const
{
    return (base_type::calculateButtonsVisibility()
        | Qn::FullscreenButton | Qn::BackButton | Qn::ReloadPageButton);
}

Qn::RenderStatus QnWebResourceWidget::paintChannelBackground(QPainter* painter, int channel,
    const QRectF& channelRect, const QRectF& paintRect)
{
    if (!paintRect.isValid())
        return Qn::NothingRendered;

    return base_type::paintChannelBackground(painter, channel, channelRect, paintRect);
}

QString QnWebResourceWidget::calculateDetailsText() const
{
    NX_ASSERT(m_webEngineView);
    static constexpr int kMaxUrlDisplayLength = 96;
    // Truncating URL if it is too long to display properly
    // I could strip query part from URL, but some sites, like youtibe will be stripped too much
    QString details = m_webEngineView->url().toString();
    return nx::utils::elideString(details, kMaxUrlDisplayLength);
}

QPixmap QnWebResourceWidget::calculateDetailsIcon() const
{
    return m_validCertificate ? QPixmap() : qnSkin->pixmap("item/lock_disabled.svg");
}

nx::vms::client::desktop::GraphicsWebEngineView* QnWebResourceWidget::webView() const
{
    return m_webEngineView.get();
}

bool QnWebResourceWidget::verifyCertificate(const QString& pemString, const QUrl& url)
{
    // Certificate have not passed validation if this method is called.
    m_validCertificate = false;
    updateDetailsText();

    auto webResource = resource().dynamicCast<QnWebPageResource>();
    if (!NX_ASSERT(webResource))
        return false;

    if (!webResource->isCertificateCheckEnabled())
        return true;

    if (pemString.isEmpty())
        return false;

    const auto chain = nx::network::ssl::Certificate::parse(pemString.toStdString());
    if (chain.empty())
        return false;

    return askUserToAcceptCertificate(chain, nx::utils::Url::fromQUrl(url));
}

bool QnWebResourceWidget::askUserToAcceptCertificate(
    const nx::network::ssl::CertificateChain& chain, const nx::utils::Url& url)
{
    QnSessionAwareMessageBox warning(mainWindowWidget());
    warning.setText(tr("Open this web page?"));
    warning.setIcon(QnMessageBox::Icon::Question);

    const QString addressLine =
        nx::vms::common::html::bold(tr("Web page")).append(": ").append(url.toDisplayString());
    const QString infoText = tr("You try to open the\n%1\nbut this web page presented an"
        " untrusted certificate auth.\nWe recommend you not to open this web page. If you"
        " understand the risks, you can open the web page.", "%1 is the web page address")
            .arg(addressLine);
    warning.setInformativeText(infoText);

    warning.addButton(
        tr("Connect anyway"), QDialogButtonBox::AcceptRole, Qn::ButtonAccent::Warning);

    warning.setStandardButtons({QDialogButtonBox::Cancel});
    warning.button(QDialogButtonBox::Cancel)->setFocus();

    if (menu()->canTrigger(ui::action::WebPageSettingsAction, resource()))
    {
        auto settingsLink = new QLabel(
            nx::vms::common::html::localLink(tr("Web page settings...")));
        warning.addCustomWidget(settingsLink, QnMessageBox::Layout::ButtonBox);
        connect(settingsLink, &QLabel::linkActivated, this,
            [this]()
            {
                menu()->triggerIfPossible(ui::action::WebPageSettingsAction, resource());
            });
    }

    return warning.exec() == QDialogButtonBox::AcceptRole
        || !resource().dynamicCast<QnWebPageResource>()->isCertificateCheckEnabled();
}
