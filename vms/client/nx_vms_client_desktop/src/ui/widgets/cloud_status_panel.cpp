// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "cloud_status_panel.h"

#include <QtWidgets/QApplication>
#include <QtWidgets/QMenu>

#include <nx/branding.h>
#include <nx/vms/client/core/network/cloud_status_watcher.h>
#include <nx/vms/client/core/skin/color_theme.h>
#include <nx/vms/client/core/skin/skin.h>
#include <nx/vms/client/core/utils/geometry.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/help/help_topic.h>
#include <nx/vms/client/desktop/help/help_topic_accessor.h>
#include <nx/vms/client/desktop/menu/action_manager.h>
#include <nx/vms/client/desktop/style/custom_style.h>
#include <nx/vms/client/desktop/style/helper.h>
#include <nx/vms/client/desktop/window_context.h>
#include <ui/common/palette.h>
#include <ui/workaround/hidpi_workarounds.h>

// TODO: #dklychkov Uncomment when cloud login is implemented
//#define DIRECT_CLOUD_CONNECT

using namespace nx::vms::client::desktop;
using nx::vms::client::core::CloudStatusWatcher;

namespace {

static const int kFontPixelSize = 12;
static const int kMinimumPanelWidth = 36;

}

class QnCloudStatusPanelPrivate: public QObject
{
    QnCloudStatusPanel* q_ptr;
    Q_DECLARE_PUBLIC(QnCloudStatusPanel)
public:
    QnCloudStatusPanelPrivate(QnCloudStatusPanel* parent);

    void updateUi();
#ifdef DIRECT_CLOUD_CONNECT
    void updateSystems();
#endif

public:
    QMenu* loggedInMenu;
    QMenu* offlineMenu;
#ifdef DIRECT_CLOUD_CONNECT
    QMenu* systemsMenu;
#endif
    QIcon loggedInIcon;     /*< User is logged in. */
    QIcon loggedOutIcon;    /*< User is logged out. */
    QIcon offlineIcon;      /*< User is logged in, but cloud is unreachable. */
};

QnCloudStatusPanel::QnCloudStatusPanel(
    nx::vms::client::desktop::WindowContext* windowContext,
    QWidget* parent)
    :
    base_type(parent),
    WindowContextAware(windowContext),
    d_ptr(new QnCloudStatusPanelPrivate(this))
{
    Q_D(QnCloudStatusPanel);

    setProperty(nx::style::Properties::kDontPolishFontProperty, true);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Minimum);
    setPaletteColor(this, QPalette::Window, nx::vms::client::core::colorTheme()->color("dark7"));
    setContentsMargins(10, 0, 0, 0);

    QFont font = qApp->font();
    font.setPixelSize(kFontPixelSize);
    font.setWeight(QFont::Bold);
    setFont(font);

    setHelpTopic(this, HelpTopic::Id::MainWindow_TitleBar_Cloud);

    connect(this, &QnCloudStatusPanel::justPressed, qnCloudStatusWatcher,
        &CloudStatusWatcher::updateSystems);

    auto showMenu =
        [this, d]
        {
            if (qnCloudStatusWatcher->status() == CloudStatusWatcher::LoggedOut)
            {
                this->windowContext()->menu()->trigger(menu::LoginToCloud);
                return;
            }

            const auto menu =
                [d]() -> QMenu*
                {
                    switch (qnCloudStatusWatcher->status())
                    {
                        case CloudStatusWatcher::Online:
                            return d->loggedInMenu;
                        case CloudStatusWatcher::Offline:
                            return d->offlineMenu;
                        default:
                            return nullptr;
                    }
                }();

            if (!menu)
                return;

            QnHiDpiWorkarounds::showMenu(menu,
                QnHiDpiWorkarounds::safeMapToGlobal(this, rect().bottomLeft()));
        };

    connect(this, &QnCloudStatusPanel::clicked, this, showMenu);

    QMenu* mockMenu = new QMenu(this);
    setMenu(mockMenu);
    connect(mockMenu, &QMenu::aboutToShow, this, showMenu);

    d->updateUi();
}

QnCloudStatusPanel::~QnCloudStatusPanel()
{
}

QSize QnCloudStatusPanel::minimumSizeHint() const
{
    auto base = base_type::minimumSizeHint();
    base = nx::vms::client::core::Geometry::dilated(base, contentsMargins());
    if (base.width() < kMinimumPanelWidth)
        base.setWidth(kMinimumPanelWidth);
    return base;
}

QSize QnCloudStatusPanel::sizeHint() const
{
    auto base = base_type::sizeHint();
    if (base.width() < kMinimumPanelWidth)
        base.setWidth(kMinimumPanelWidth);
    return base;
}

QnCloudStatusPanelPrivate::QnCloudStatusPanelPrivate(QnCloudStatusPanel* parent):
    QObject(parent),
    q_ptr(parent),
    loggedInMenu(new QMenu(parent->mainWindowWidget())),
    offlineMenu(new QMenu(parent->mainWindowWidget())),
#ifdef DIRECT_CLOUD_CONNECT
    systemsMenu(nullptr),
#endif
    loggedInIcon(qnSkin->icon("cloud/cloud_20_selected.png")),
    loggedOutIcon(qnSkin->icon("cloud/cloud_20_disabled.png")),
    offlineIcon(qnSkin->icon("cloud/cloud_20_offline_disabled.png"))
{
    Q_Q(QnCloudStatusPanel);
    loggedInMenu->setWindowFlags(loggedInMenu->windowFlags());
    loggedInMenu->addAction(q->action(menu::OpenCloudMainUrl));
    loggedInMenu->addAction(q->action(menu::OpenCloudManagementUrl));
    loggedInMenu->addAction(q->action(menu::LogoutFromCloud));

    auto offlineAction = new QAction(this);
    offlineAction->setText(QnCloudStatusPanel::tr("Cannot connect to %1",
        "%1 is the cloud name (like Nx Cloud)").arg(nx::branding::cloudName()));
    offlineAction->setEnabled(false);

    offlineMenu->setWindowFlags(offlineMenu->windowFlags());
    offlineMenu->addAction(offlineAction);
    offlineMenu->addSeparator();
    offlineMenu->addAction(q->action(menu::LogoutFromCloud));

    connect(qnCloudStatusWatcher, &CloudStatusWatcher::statusChanged, this,
        &QnCloudStatusPanelPrivate::updateUi);
    connect(qnCloudStatusWatcher, &CloudStatusWatcher::cloudLoginChanged, this,
        &QnCloudStatusPanelPrivate::updateUi);

#ifdef DIRECT_CLOUD_CONNECT
    systemsMenu = loggedInMenu->addMenu(QnCloudStatusPanel::tr("Connect to Server"));
    connect(qnCloudStatusWatcher, &CloudStatusWatcher::cloudSystemsChanged, this, &QnCloudStatusPanelPrivate::updateSystems);
    updateSystems();
#endif
}

void QnCloudStatusPanelPrivate::updateUi()
{
    Q_Q(QnCloudStatusPanel);

    QString effectiveUserName = qnCloudStatusWatcher->cloudLogin();
    if (effectiveUserName.isEmpty())
        effectiveUserName = QnCloudStatusPanel::tr("Logging in...");

    switch (qnCloudStatusWatcher->status())
    {
        case CloudStatusWatcher::LoggedOut:
            q->setText(QString());
            q->setIcon(loggedOutIcon);
            q->setToolButtonStyle(Qt::ToolButtonIconOnly);
            q->setPopupMode(QToolButton::DelayedPopup);
            break;
        case CloudStatusWatcher::Online:
            q->setText(effectiveUserName);
            q->setIcon(loggedInIcon);
            q->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
            q->setPopupMode(QToolButton::MenuButtonPopup);
            break;
        case CloudStatusWatcher::Offline:
            q->setText(effectiveUserName);
            q->setIcon(offlineIcon);
            q->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
            q->setPopupMode(QToolButton::MenuButtonPopup);
            break;
        default:
            NX_ASSERT(false, "Should never get here.");
            break;
    }
    q->adjustIconSize();
}

#ifdef DIRECT_CLOUD_CONNECT
void QnCloudStatusPanelPrivate::updateSystems()
{
    const QnCloudSystemList& systems = qnCloudStatusWatcher->cloudSystems();

    systemsMenu->clear();
    for (const QnCloudSystem& system : systems)
    {
        QAction* action = systemsMenu->addAction(system.name);
        QUrl url;
        // TODO: #dklychkov Prepare URL
        action->setData(url);
    }

    systemsMenu->menuAction()->setVisible(!systemsMenu->actions().isEmpty());
}
#endif
