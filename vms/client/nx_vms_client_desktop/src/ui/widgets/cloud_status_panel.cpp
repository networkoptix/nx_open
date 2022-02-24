// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "cloud_status_panel.h"

#include <QtWidgets/QApplication>
#include <QtWidgets/QMenu>

#include <nx/branding.h>
#include <nx/vms/client/desktop/style/custom_style.h>
#include <nx/vms/client/desktop/style/helper.h>
#include <nx/vms/client/desktop/style/skin.h>
#include <nx/vms/client/desktop/ui/actions/action_manager.h>
#include <nx/vms/client/desktop/ui/common/color_theme.h>
#include <ui/common/palette.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>
#include <ui/workaround/hidpi_workarounds.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_display.h>
#include <watchers/cloud_status_watcher.h>

// TODO: #dklychkov Uncomment when cloud login is implemented
//#define DIRECT_CLOUD_CONNECT

using namespace nx::vms::client::desktop;
using namespace nx::vms::client::desktop::ui;

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

QnCloudStatusPanel::QnCloudStatusPanel(QWidget* parent):
    base_type(parent),
    QnWorkbenchContextAware(parent),
    d_ptr(new QnCloudStatusPanelPrivate(this))
{
    Q_D(QnCloudStatusPanel);

    setProperty(nx::style::Properties::kDontPolishFontProperty, true);
    setPopupMode(QToolButton::InstantPopup);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Minimum);
    setPaletteColor(this, QPalette::Window, colorTheme()->color("dark8"));

    QFont font = qApp->font();
    font.setPixelSize(kFontPixelSize);
    font.setWeight(QFont::Bold);
    setFont(font);

    setHelpTopic(this, Qn::MainWindow_TitleBar_Cloud_Help);

    connect(this, &QnCloudStatusPanel::justPressed, qnCloudStatusWatcher,
        &QnCloudStatusWatcher::updateSystems);

    connect(this, &QnCloudStatusPanel::clicked, this,
        [this, d]
        {
            if (qnCloudStatusWatcher->status() == QnCloudStatusWatcher::LoggedOut)
            {
                context()->menu()->trigger(action::LoginToCloud);
                return;
            }

            const auto menu =
                [d]() -> QMenu*
                {
                    switch (qnCloudStatusWatcher->status())
                    {
                        case QnCloudStatusWatcher::Online:
                            return d->loggedInMenu;
                        case QnCloudStatusWatcher::Offline:
                            return d->offlineMenu;
                        default:
                            return nullptr;
                    }
                }();

            if (!menu)
                return;

            QnHiDpiWorkarounds::showMenu(menu,
                QnHiDpiWorkarounds::safeMapToGlobal(this, rect().bottomLeft()));
        });

    d->updateUi();
}

QnCloudStatusPanel::~QnCloudStatusPanel()
{
}

QSize QnCloudStatusPanel::minimumSizeHint() const
{
    auto base = base_type::minimumSizeHint();
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
    loggedInMenu->addAction(q->action(action::OpenCloudMainUrl));
    loggedInMenu->addSeparator();
    loggedInMenu->addAction(q->action(action::OpenCloudManagementUrl));
    loggedInMenu->addAction(q->action(action::LogoutFromCloud));

    auto offlineAction = new QAction(this);
    offlineAction->setText(QnCloudStatusPanel::tr("Cannot connect to %1",
        "%1 is the cloud name (like Nx Cloud)").arg(nx::branding::cloudName()));
    offlineAction->setEnabled(false);

    offlineMenu->setWindowFlags(offlineMenu->windowFlags());
    offlineMenu->addAction(offlineAction);
    offlineMenu->addSeparator();
    offlineMenu->addAction(q->action(action::LogoutFromCloud));

    connect(qnCloudStatusWatcher, &QnCloudStatusWatcher::statusChanged, this,
        &QnCloudStatusPanelPrivate::updateUi);
    connect(qnCloudStatusWatcher, &QnCloudStatusWatcher::cloudLoginChanged, this,
        &QnCloudStatusPanelPrivate::updateUi);

#ifdef DIRECT_CLOUD_CONNECT
    systemsMenu = loggedInMenu->addMenu(QnCloudStatusPanel::tr("Connect to Server"));
    connect(qnCloudStatusWatcher, &QnCloudStatusWatcher::cloudSystemsChanged, this, &QnCloudStatusPanelPrivate::updateSystems);
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
        case QnCloudStatusWatcher::LoggedOut:
            q->setText(QString());
            q->setIcon(loggedOutIcon);
            q->setToolButtonStyle(Qt::ToolButtonIconOnly);
            break;
        case QnCloudStatusWatcher::Online:
            q->setText(effectiveUserName);
            q->setIcon(loggedInIcon);
            q->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
            break;
        case QnCloudStatusWatcher::Offline:
            q->setText(effectiveUserName);
            q->setIcon(offlineIcon);
            q->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
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
