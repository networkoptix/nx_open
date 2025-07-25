// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "web_resource_widget.h"

#include <QtCore/QTimer>
#include <QtWidgets/QGraphicsView>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>

#include <core/resource/webpage_resource.h>
#include <nx/network/ssl/certificate.h>
#include <nx/utils/string.h>
#include <nx/utils/url.h>
#include <nx/vms/client/core/resource/layout_resource.h>
#include <nx/vms/client/core/skin/skin.h>
#include <nx/vms/client/desktop/common/widgets/webview_controller.h>
#include <nx/vms/client/desktop/help/help_topic.h>
#include <nx/vms/client/desktop/ini.h>
#include <nx/vms/client/desktop/menu/action_manager.h>
#include <nx/vms/client/desktop/ui/dialogs/client_api_auth_dialog.h>
#include <nx/vms/client/desktop/ui/dialogs/web_page_certificate_dialog.h>
#include <nx/vms/client/desktop/workbench/workbench.h>
#include <nx/vms/common/html/html.h>
#include <nx/vms/text/human_readable.h>
#include <ui/dialogs/common/session_aware_dialog.h>
#include <ui/graphics/items/generic/image_button_bar.h>
#include <ui/graphics/items/generic/image_button_widget.h>
#include <ui/graphics/items/overlays/hud_overlay_widget.h>
#include <ui/graphics/items/overlays/resource_title_item.h>
#include <ui/graphics/items/resource/button_ids.h>
#include <ui/graphics/items/standard/graphics_web_view.h>
#include <ui/widgets/main_window.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_item.h>
#include <utils/common/delayed.h>

using namespace nx::vms::client::desktop;
using namespace std::chrono;

namespace {

const auto kDefaultWidgetMargins = QMargins(1, 36, 1, 1);

const nx::vms::client::core::SvgIconColorer::ThemeSubstitutions kLight1Theme = {
    {QIcon::Normal, {.primary = "light1"}}
};

NX_DECLARE_COLORIZED_ICON(kBackIcon, "24x24/Outline/back.svg", kLight1Theme)
NX_DECLARE_COLORIZED_ICON(kRefreshAutoIcon, "24x24/Outline/refresh_auto.svg", kLight1Theme)
NX_DECLARE_COLORIZED_ICON(kRefreshIcon, "24x24/Outline/refresh.svg", kLight1Theme)
NX_DECLARE_COLORIZED_ICON(kLockDisabledIcon, "16x16/Outline/lock_disabled.svg", kLight1Theme)

} // namespace

QnWebResourceWidget::QnWebResourceWidget(
    SystemContext* systemContext,
    WindowContext* windowContext,
    QnWorkbenchItem* item,
    QGraphicsItem* parent)
    :
    base_type(systemContext, windowContext, item, parent),
    m_webEngineView(item->data(Qn::ItemWebPageSavedStateDataRole).isNull()
        ? new GraphicsWebEngineView(resource(), this)
        : new GraphicsWebEngineView({}, this)), //< Page will be loaded from saved state later.
    m_refreshTimer(new QTimer())
{
    setOption(AllowFocus, false);

    m_webEngineView->installEventFilter(this);
    m_webEngineView->controller()->setCertificateValidator(
        [this](const QString& pemString, const QUrl& url)
        {
            bool result = verifyCertificate(pemString, url);
            if (!result)
            {
                // Stay on current page, if it was loaded, close widget asynchronously otherwise
                // to avoid destroying while in the QML handler.
                if (m_pageLoaded)
                    m_webEngineView->controller()->stop();
                else
                    executeLater([this](){ close(); }, this);
            }
            return result;
        });

    setupWidget();
    setFocusProxy(m_webEngineView.get());
    connect(this, &QGraphicsWidget::geometryChanged,
        m_webEngineView.get(), &GraphicsQmlView::updateWindowGeometry);

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

    connect(m_webEngineView.get(), &GraphicsWebEngineView::urlChanged,
        this, &QnWebResourceWidget::updateDetailsText);
    connect(m_webEngineView.get(), &GraphicsWebEngineView::statusChanged,
        this, updateStatusesHandler);
    connect(m_webEngineView.get(), &GraphicsWebEngineView::loadFinished, this,
        [this](bool ok)
        {
            if (ok)
                m_pageLoaded = true;
        });

    connect(m_refreshTimer.get(), &QTimer::timeout,
        m_webEngineView.get(), &GraphicsWebEngineView::reload);

    connect(layoutResource().get(), &QnLayoutResource::lockedChanged,
        this, &QnWebResourceWidget::setupWidget);

    setupOverlays();
    disableHotkeyHints();
    updateButtonsVisibility();

    const auto controller = m_webEngineView->controller();

    // May be used with another QnWebResourceWidget.
    auto authCondition =
        [resource = resource()](const QUrl& url)
        {
            return ClientApiAuthDialog::isApproved(resource, url);
        };

    controller->initClientApiSupport(this->item(), authCondition);
    controller->setLoadIcon(true);

    const auto webPage = resource().dynamicCast<QnWebPageResource>();
    if (!webPage->isOpenInWindow())
    {
        connect(webPage.get(), &QnWebPageResource::dedicatedWindowSettingsChanged, this,
            [this, webPage]()
            {
                if (webPage->isOpenInWindow())
                    moveToDedicatedWindow();
            });
    }
}

QnWebResourceWidget::~QnWebResourceWidget()
{
    /* statusChanged will be emitted during the destruction process. */
    m_webEngineView->disconnect(this);
}

int QnWebResourceWidget::helpTopicAt(const QPointF& /*pos*/) const
{
    return HelpTopic::Id::MainWindow_WebPageItem;
}

void QnWebResourceWidget::setupOverlays()
{
    {
        // Left buttons bar setup
        auto buttonsBar = titleBar()->leftButtonsBar();

        auto backButton = createStatisticAwareButton(lit("web_widget_back"));
        backButton->setIcon(qnSkin->icon(kBackIcon));
        backButton->setObjectName("BackButton");
        connect(backButton, &QnImageButtonWidget::clicked, m_webEngineView.get(), &GraphicsWebEngineView::back);
        buttonsBar->addButton(Qn::BackButton, backButton);
        buttonsBar->setButtonsEnabled(Qn::BackButton, false);

        connect(m_webEngineView.get(), &GraphicsWebEngineView::canGoBackChanged, this,
            [this, buttonsBar]()
            {
                buttonsBar->setButtonsEnabled(Qn::BackButton, m_webEngineView->canGoBack());
            });

        // Should force HUD to update details text with new URL.
        connect(m_webEngineView.get(), &GraphicsWebEngineView::loadStarted, this,
            [this]()
            {
                this->updateDetailsText();
            });

        auto webResource = qobject_cast<QnWebPageResource*>(resource().get());
        auto reloadButton = createStatisticAwareButton(lit("web_widget_reload"));
        const auto updateRefreshInterval =
            [webResource, reloadButton, this]()
            {
                const bool isAutoRefreshEnabled = webResource->refreshInterval().count() > 0;

                reloadButton->setIcon(
                    qnSkin->icon(isAutoRefreshEnabled ? kRefreshAutoIcon : kRefreshIcon));

                m_refreshTimer->setInterval(
                    duration_cast<milliseconds>(webResource->refreshInterval()));

                if (isAutoRefreshEnabled)
                    m_refreshTimer->start();
                else
                    m_refreshTimer->stop();

                if (isAutoRefreshEnabled)
                {
                    using namespace nx::vms::text;
                    reloadButton->setToolTip(tr("Auto-refresh every %1").arg(
                        HumanReadable::timeSpan(
                            duration_cast<milliseconds>(webResource->refreshInterval()),
                            HumanReadable::Minutes | HumanReadable::Seconds,
                            HumanReadable::SuffixFormat::Short,
                            /*separator*/ {},
                            HumanReadable::kAlwaysSuppressSecondUnit)));
                }
                else
                {
                    reloadButton->setToolTip(tr("Refresh"));
                }
            };

        connect(webResource, &QnWebPageResource::refreshIntervalChanged,
            this, updateRefreshInterval);

        updateRefreshInterval();

        reloadButton->setObjectName("ReloadButton");
        connect(reloadButton, &QnImageButtonWidget::clicked,
            m_webEngineView.get(), &GraphicsWebEngineView::reload);

        buttonsBar->addButton(Qn::ReloadPageButton, reloadButton);
    }
}

void QnWebResourceWidget::disableHotkeyHints()
{
    // Reset button tooltips to a basic tooltip text without hotkey hints.
    auto closeButton = titleBar()->rightButtonsBar()->button(Qn::CloseButton);
    if (NX_ASSERT(closeButton))
        closeButton->setToolTip(closeButtonTooltip());
    auto infoButton = titleBar()->rightButtonsBar()->button(Qn::InfoButton);
    if (NX_ASSERT(infoButton))
        infoButton->setToolTip(infoButtonTooltip());
}

void QnWebResourceWidget::setupWidget()
{
    removeOverlayWidget(m_webEngineView.get());

    const bool hasButtons = calculateButtonsVisibility() != 0;
    const auto contentMargins = hasButtons ? kDefaultWidgetMargins : QMargins{};
    const auto webParams =
        detail::OverlayParams(Visible, OverlayFlag::bindToViewport, BaseLayer, contentMargins);

    addOverlayWidget(m_webEngineView.get(), webParams);

    titleBar()->setVisible(hasButtons);
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
            [this]() { return mainWindow()->view()->hasFocus(); };

        if (event->type() == QEvent::GraphicsSceneMousePress)
        {
            auto mouseEvent = static_cast<QGraphicsSceneMouseEvent*>(event);
            mousePressEvent(mouseEvent);
            if (!m_webEngineView->controller()->isWebPageContextMenuEnabled())
            {
                if (mouseEvent->button() == Qt::RightButton)
                    return true;
            }
        }
        if (m_isMinimalTitleBar && event->type() == QEvent::GraphicsSceneHoverMove)
        {
            const auto mouseEvent = static_cast<QGraphicsSceneMouseEvent*>(event);
            const bool isTitleBarHovered = titleBar()->geometry().contains(mouseEvent->pos());

            // Moving a widget is only possible when the GraphicsWebEngineView accepts no buttons.
            m_webEngineView->setAcceptedMouseButtons(
                isTitleBarHovered ? Qt::NoButton : Qt::AllButtons);
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
    if (!m_overlayIsVisible)
        return false;

    const int defaultButtonsVisibility = base_type::calculateButtonsVisibility();
    return m_isMinimalTitleBar
        ? defaultButtonsVisibility & Qn::CloseButton
        : (defaultButtonsVisibility
            | Qn::FullscreenButton | Qn::BackButton | Qn::ReloadPageButton);
}

QString QnWebResourceWidget::calculateTitleText() const
{
    return m_isMinimalTitleBar ? "" : base_type::calculateTitleText();
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
    // I could strip query part from URL, but some sites, like youtube will be stripped too much
    QString details = m_webEngineView->url().toString();
    return nx::utils::elideString(details, kMaxUrlDisplayLength);
}

QPixmap QnWebResourceWidget::calculateDetailsIcon() const
{
    return m_validCertificate ? QPixmap() : qnSkin->icon(kLockDisabledIcon).pixmap(16, 16);
}

nx::vms::client::desktop::GraphicsWebEngineView* QnWebResourceWidget::webView() const
{
    return m_webEngineView.get();
}

void QnWebResourceWidget::setMinimalTitleBarMode(bool value)
{
    m_isMinimalTitleBar = value;
    setupWidget();
    updateButtonsVisibility();
    updateTitleText();
}

bool QnWebResourceWidget::overlayIsVisible() const
{
    return m_overlayIsVisible;
}

void QnWebResourceWidget::setOverlayVisibility(bool visible)
{
    m_overlayIsVisible = visible;

    const bool hasButtons = calculateButtonsVisibility() != 0;
    const auto contentMargins = hasButtons ? kDefaultWidgetMargins : QMargins{};
    updateOverlayWidgetMargins(m_webEngineView.get(), contentMargins);
    setOverlayWidgetVisibility(
        hudOverlay(),
        hasButtons ? OverlayVisibility::Visible : OverlayVisibility::Invisible,
        true);
}

void QnWebResourceWidget::setPreventDefaultContextMenu(bool value)
{
    m_webEngineView->controller()->setWebPageContextMenuEnabled(value);
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

    return askUserToAcceptCertificate(chain, nx::Url::fromQUrl(url));
}

bool QnWebResourceWidget::askUserToAcceptCertificate(
    const nx::network::ssl::CertificateChain& chain, const nx::Url& url)
{
    const auto webPage = resource().dynamicCast<QnWebPageResource>();
    const bool isIntegration = ini().webPagesAndIntegrations
        && webPage->getOptions().testFlag(QnWebPageResource::Integration);

    WebPageCertificateDialog warning(mainWindowWidget(), url.toQUrl(), isIntegration);

    const auto settingsAction =
        isIntegration ? menu::IntegrationSettingsAction : menu::WebPageSettingsAction;

    if (menu()->canTrigger(settingsAction, webPage))
    {
        auto settingsLink = new QLabel(
            nx::vms::common::html::localLink(
                isIntegration ? tr("Integration settings...") : tr("Web Page settings...")));

        warning.addCustomWidget(settingsLink, QnMessageBox::Layout::ButtonBox);
        connect(settingsLink, &QLabel::linkActivated, this,
            [this, settingsAction]()
            {
                menu()->triggerIfPossible(settingsAction, resource());
            });
    }

    return warning.exec() == QDialogButtonBox::AcceptRole
        || !webPage->isCertificateCheckEnabled();
}
