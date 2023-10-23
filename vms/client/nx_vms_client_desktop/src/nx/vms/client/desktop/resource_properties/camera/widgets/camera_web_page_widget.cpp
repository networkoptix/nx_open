// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "camera_web_page_widget.h"

#include <chrono>

#include <QtCore/QElapsedTimer>
#include <QtCore/QThread>
#include <QtQuick/QQuickItem>
#include <QtWebEngineQuick/QQuickWebEngineProfile>
#include <QtWidgets/QApplication>
#include <QtWidgets/QLabel>

#include <client_core/client_core_module.h>
#include <core/resource/camera_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/vms/client/core/access/access_controller.h>
#include <nx/vms/client/core/skin/color_theme.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/common/utils/camera_web_authenticator.h>
#include <nx/vms/client/desktop/common/utils/widget_anchor.h>
#include <nx/vms/client/desktop/common/widgets/web_widget.h>
#include <nx/vms/client/desktop/help/help_topic.h>
#include <nx/vms/client/desktop/help/help_topic_accessor.h>
#include <nx/vms/client/desktop/resource_properties/camera/utils/camera_web_page_workarounds.h>
#include <nx/vms/client/desktop/system_context.h>
#include <ui/common/palette.h>
#include <utils/common/event_processors.h>

#include "../flux/camera_settings_dialog_state.h"
#include "../flux/camera_settings_dialog_store.h"

namespace {

bool currentlyOnUiThread()
{
    return QThread::currentThread() == qApp->thread();
}

} // namespace

namespace nx::vms::client::desktop {

struct CameraWebPageWidget::Private
{
    Private(CameraWebPageWidget* parent);

    void loadPage();

    void resetPage();

    WebWidget* const webWidget;
    QLabel* const noAccessWidget;

    CameraWebPageWidget* parent;
    QString cameraLogin;
    QUrl lastRequestUrl;
    CameraSettingsDialogState::SingleCameraProperties lastCamera;
    bool pendingLoadPage = false;
};

CameraWebPageWidget::Private::Private(CameraWebPageWidget* parent):
    webWidget(new WebWidget(parent)),
    noAccessWidget(new QLabel(parent)),
    parent(parent)
{
    anchorWidgetToParent(webWidget);
    anchorWidgetToParent(noAccessWidget);

    noAccessWidget->setText("NO ACCESS");
    auto font = noAccessWidget->font();
    font.setCapitalization(QFont::AllUppercase);
    font.setPixelSize(32);
    font.setWeight(QFont::Normal);
    noAccessWidget->setFont(font);
    noAccessWidget->setAlignment(Qt::AlignCenter);
    setPaletteColor(
        noAccessWidget, noAccessWidget->foregroundRole(), core::colorTheme()->color("red_core"));

    installEventHandler(parent, QEvent::Show, parent,
        [this]()
        {
            if (!pendingLoadPage)
                return;

            loadPage();
        });

    // For each camera a new profile is created in order to isolate proxy settings.
    // Disable saving profile to disk and avoid excessive space consumption.
    webWidget->controller()->setOffTheRecord(true);

    webWidget->controller()->setMenuNavigation(true);
    webWidget->controller()->setMenuSave(true);

    // We don't have any info about cameras certificates right now.
    // Assume that we can trust them as long as they are a part of our system already.
    webWidget->controller()->setCertificateValidator(
        [](const QString& /*certificateChain*/, const QUrl&){ return true; });
}

CameraWebPageWidget::CameraWebPageWidget(
    SystemContext* systemContext,
    CameraSettingsDialogStore* store,
    QWidget* parent)
    :
    base_type(parent),
    SystemContextAware(systemContext),
    d(new Private(this))
{
    NX_ASSERT(store);
    connect(store, &CameraSettingsDialogStore::stateChanged, this, &CameraWebPageWidget::loadState);

    setHelpTopic(this, HelpTopic::Id::CameraSettingsWebPage);

    connect(d->webWidget->controller(), &WebViewController::profileChanged, this,
        [this](QQuickWebEngineProfile* profile)
        {
            if (!d->lastCamera.overrideHttpUserAgent.isNull())
                profile->setHttpUserAgent(d->lastCamera.overrideHttpUserAgent);

            if (d->lastCamera.overrideXmlHttpRequestTimeout > 0)
            {
                CameraWebPageWorkarounds::setXmlHttpRequestTimeout(
                    d->webWidget->controller(),
                    std::chrono::milliseconds(d->lastCamera.overrideXmlHttpRequestTimeout));
            }
        });
}

CameraWebPageWidget::~CameraWebPageWidget()
{
    // Required here for forward-declared scoped pointer destruction.
}

void CameraWebPageWidget::keyPressEvent(QKeyEvent* event)
{
    base_type::keyPressEvent(event);

    // Web page JavaScript code observes and reacts on key presses, but may not accept the actual
    // event. In regular web browser pressing those keys does nothing, but in camera settings web
    // page dialog they will close the dialog and the user won't be able interact with the web
    // page any further.
    // So just accept them anyway in order to provide the same user experience as in regular
    // web browser.
    switch (event->key())
    {
        case Qt::Key_Return:
        case Qt::Key_Enter:
        case Qt::Key_Escape:
            event->setAccepted(true);
            break;
        default:
            break;
    }
}

void CameraWebPageWidget::cleanup()
{
    d->resetPage();
}

void CameraWebPageWidget::Private::resetPage()
{
    if (!lastRequestUrl.isValid())
        return;

    webWidget->controller()->closeWindows();
    webWidget->controller()->load(QUrl());

    lastRequestUrl = QUrl();
    lastCamera = {};
}

void CameraWebPageWidget::Private::loadPage()
{
    const auto resourcePool = qnClientCoreModule->resourcePool();

    // Cannot load resource url directly because camera settings url can contain a path from
    // resource_data.json and it is resolved in loadState().
    webWidget->controller()->load(
        resourcePool->getResourceById<QnVirtualCameraResource>(lastCamera.id),
        lastRequestUrl);

    pendingLoadPage = false;
}

void CameraWebPageWidget::loadState(const CameraSettingsDialogState& state)
{
    NX_ASSERT(currentlyOnUiThread());

    if (!state.canShowWebPage())
    {
        d->resetPage();
        return;
    }

    d->cameraLogin = state.credentials.login.valueOr(QString());
    const auto cameraId = state.singleCameraProperties.id;
    const auto camera = resourcePool()->getResourceById<QnVirtualCameraResource>(cameraId);

    if (!camera) //< Camera was just deleted.
    {
        d->resetPage();
        return;
    }

    const QUrl targetUrl = state.singleCameraProperties.settingsUrl;

    NX_ASSERT(targetUrl.isValid());

    bool hasPermissions = state.hasExportPermission || !state.screenRecordingOn;
    if (d->lastRequestUrl == targetUrl
        && cameraId == d->lastCamera.id
        && d->webWidget->isVisible() == hasPermissions)
    {
        return;
    }

    d->lastRequestUrl = targetUrl;
    d->lastCamera = state.singleCameraProperties;
    d->webWidget->setVisible(hasPermissions);
    d->noAccessWidget->setVisible(!hasPermissions);
    if (hasPermissions)
    {
        d->webWidget->controller()->setAuthenticator(
            std::make_shared<CameraWebAuthenticator>(
                camera,
                targetUrl,
                state.credentials.login.valueOr(QString{}),
                state.credentials.password.valueOr(QString{})));

        d->webWidget->controller()->closeWindows();

        if (isVisible())
            d->loadPage();
        else
            d->pendingLoadPage = true;
    }
}

} // namespace nx::vms::client::desktop
