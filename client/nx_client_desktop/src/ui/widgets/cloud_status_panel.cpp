#include "cloud_status_panel.h"

#include <QtWidgets/QApplication>
#include <QtWidgets/QMenu>

#include <nx/network/app_info.h>

#include <ui/common/palette.h>
#include <ui/style/custom_style.h>
#include <ui/style/helper.h>
#include <ui/style/skin.h>
#include <utils/common/app_info.h>
#include <ui/workaround/hidpi_workarounds.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_display.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>

#include <watchers/cloud_status_watcher.h>

// TODO: #dklychkov Uncomment when cloud login is implemented
//#define DIRECT_CLOUD_CONNECT

using namespace nx::client::desktop::ui;

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

    setProperty(style::Properties::kDontPolishFontProperty, true);
    setPopupMode(QToolButton::InstantPopup);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Minimum);

    QFont font = qApp->font();
    font.setPixelSize(kFontPixelSize);
    font.setWeight(QFont::Bold);
    setFont(font);

    setHelpTopic(this, Qn::MainWindow_TitleBar_Cloud_Help);

    connect(this, &QnCloudStatusPanel::justPressed, qnCloudStatusWatcher,
        &QnCloudStatusWatcher::updateSystems);

    connect(this, &QnCloudStatusPanel::clicked, this,
        [this, parent, d]
        {
            if (qnCloudStatusWatcher->status() == QnCloudStatusWatcher::LoggedOut)
            {
                action(action::LoginToCloud)->trigger();
                return;
            }

            const auto menu =
                [this, d]() -> QMenu*
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
        "%1 is the cloud name (like 'Nx Cloud')").arg(nx::network::AppInfo::cloudName()));
    offlineAction->setEnabled(false);

    offlineMenu->setWindowFlags(offlineMenu->windowFlags());
    offlineMenu->addAction(offlineAction);
    offlineMenu->addSeparator();
    offlineMenu->addAction(q->action(action::LogoutFromCloud));

    connect(qnCloudStatusWatcher, &QnCloudStatusWatcher::statusChanged, this,
        &QnCloudStatusPanelPrivate::updateUi);
    connect(qnCloudStatusWatcher, &QnCloudStatusWatcher::effectiveUserNameChanged, this,
        &QnCloudStatusPanelPrivate::updateUi);

#ifdef DIRECT_CLOUD_CONNECT
    systemsMenu = loggedInMenu->addMenu(QnCloudStatusPanel::tr("Connect to Server..."));
    connect(qnCloudStatusWatcher, &QnCloudStatusWatcher::cloudSystemsChanged, this, &QnCloudStatusPanelPrivate::updateSystems);
    updateSystems();
#endif
}

void QnCloudStatusPanelPrivate::updateUi()
{
    Q_Q(QnCloudStatusPanel);

    QString effectiveUserName = qnCloudStatusWatcher->effectiveUserName();
    if (effectiveUserName.isEmpty())
    {
        const auto cloudLogin = qnCloudStatusWatcher->cloudLogin();
        if (cloudLogin.contains(L'@'))
            effectiveUserName = cloudLogin;// Permanent login
        else
            effectiveUserName = QnCloudStatusPanel::tr("Logging in...");
    }

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
