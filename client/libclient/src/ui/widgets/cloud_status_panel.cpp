#include "cloud_status_panel.h"

#include <QtWidgets/QMenu>

#include <ui/common/palette.h>
#include <ui/style/custom_style.h>
#include <ui/style/helper.h>
#include <ui/style/skin.h>
#include <utils/common/app_info.h>
#include <ui/workaround/hidpi_workarounds.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_display.h>

#include <watchers/cloud_status_watcher.h>

//TODO: #dklychkov Uncomment when cloud login is implemented
//#define DIRECT_CLOUD_CONNECT

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
    QColor originalTextColor;
    QIcon loggedInIcon;     /*< User is logged in. */
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
    setToolButtonStyle(Qt::ToolButtonTextBesideIcon);

    connect(this, &QnCloudStatusPanel::justPressed, qnCloudStatusWatcher,
        &QnCloudStatusWatcher::updateSystems);

    connect(this, &QnCloudStatusPanel::clicked, this,
        [this, parent, d]
        {
            if (qnCloudStatusWatcher->status() == QnCloudStatusWatcher::LoggedOut)
            {
                action(QnActions::LoginToCloud)->trigger();
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

QnCloudStatusPanelPrivate::QnCloudStatusPanelPrivate(QnCloudStatusPanel* parent):
    QObject(parent),
    q_ptr(parent),
    loggedInMenu(new QMenu(parent->mainWindow())),
    offlineMenu(new QMenu(parent->mainWindow())),
#ifdef DIRECT_CLOUD_CONNECT
    systemsMenu(nullptr),
#endif
    originalTextColor(parent->palette().color(QPalette::ButtonText)),
    loggedInIcon(qnSkin->icon("cloud/cloud_20_selected.png")),
    offlineIcon(qnSkin->icon("cloud/cloud_20_offline_disabled.png"))
{
    Q_Q(QnCloudStatusPanel);
    loggedInMenu->setWindowFlags(loggedInMenu->windowFlags());
    loggedInMenu->addAction(q->action(QnActions::OpenCloudMainUrl));
    loggedInMenu->addSeparator();
    loggedInMenu->addAction(q->action(QnActions::OpenCloudManagementUrl));
    loggedInMenu->addAction(q->action(QnActions::LogoutFromCloud));

    auto offlineAction = new QAction(this);
    offlineAction->setText(QnCloudStatusPanel::tr("Cannot connect to %1",
        "%1 is name of cloud (like 'Nx Cloud')").arg(QnAppInfo::cloudName()));
    offlineAction->setEnabled(false);

    offlineMenu->setWindowFlags(offlineMenu->windowFlags());
    offlineMenu->addAction(offlineAction);
    offlineMenu->addSeparator();
    offlineMenu->addAction(q->action(QnActions::LogoutFromCloud));

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
    static const int kFontPixelSize = 12;

    Q_Q(QnCloudStatusPanel);

    QFont font = qApp->font();
    font.setPixelSize(kFontPixelSize);


    QString effectiveUserName = qnCloudStatusWatcher->effectiveUserName();
    if (effectiveUserName.isEmpty())
    {
        const auto cloudLogin = qnCloudStatusWatcher->cloudLogin();
        if (cloudLogin.contains(L'@'))
            effectiveUserName = cloudLogin;// Permanent login
        else
            effectiveUserName = QnCloudStatusPanel::tr("Logging in...");
    }

    QColor fontColor = originalTextColor;
    switch (qnCloudStatusWatcher->status())
    {
        case QnCloudStatusWatcher::LoggedOut:
            q->setText(QnCloudStatusPanel::tr("Log in to %1...",
                "%1 is name of cloud (like 'Nx Cloud')").arg(QnAppInfo::cloudName()));
            q->setIcon(QIcon());
            font.setWeight(QFont::Normal);
            fontColor = q->palette().color(QPalette::Light);
            break;
        case QnCloudStatusWatcher::Online:
            q->setText(effectiveUserName);
            q->setIcon(loggedInIcon);
            font.setWeight(QFont::Bold);
            break;
        case QnCloudStatusWatcher::Offline:
            q->setText(effectiveUserName);
            q->setIcon(offlineIcon);
            font.setWeight(QFont::Bold);
            break;
        default:
            NX_ASSERT(false, "Should never get here.");
            break;
    }
    q->setFont(font);
    q->adjustIconSize();
    setPaletteColor(q, QPalette::ButtonText, fontColor);
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
        //TODO: #dklychkov Prepare URL
        action->setData(url);
    }

    systemsMenu->menuAction()->setVisible(!systemsMenu->actions().isEmpty());
}
#endif
