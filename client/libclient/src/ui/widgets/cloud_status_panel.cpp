#include "cloud_status_panel.h"

#include <QtWidgets/QMenu>

#include <ui/style/custom_style.h>
#include <ui/style/helper.h>
#include <ui/style/skin.h>

#include <utils/common/app_info.h>

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
    QPalette originalPalette;
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
        [this]
        {
            if (qnCloudStatusWatcher->status() == QnCloudStatusWatcher::LoggedOut)
                action(QnActions::LoginToCloud)->trigger();
        });

    d->updateUi();
}

QnCloudStatusPanel::~QnCloudStatusPanel()
{
}

QnCloudStatusPanelPrivate::QnCloudStatusPanelPrivate(QnCloudStatusPanel* parent):
    QObject(parent),
    q_ptr(parent),
    loggedInMenu(new QMenu(parent)),
    offlineMenu(new QMenu(parent)),
#ifdef DIRECT_CLOUD_CONNECT
    systemsMenu(nullptr),
#endif
    originalPalette(parent->palette()),
    loggedInIcon(qnSkin->icon("titlebar/cloud_logged.png")),
    offlineIcon(qnSkin->icon("titlebar/cloud_offline.png"))
{
    Q_Q(QnCloudStatusPanel);
    loggedInMenu->setWindowFlags(loggedInMenu->windowFlags() | Qt::BypassGraphicsProxyWidget);
    loggedInMenu->addAction(q->action(QnActions::OpenCloudMainUrl));
    loggedInMenu->addSeparator();
    loggedInMenu->addAction(q->action(QnActions::OpenCloudManagementUrl));
    loggedInMenu->addAction(q->action(QnActions::LogoutFromCloud));

    auto offlineAction = new QAction(this);
    offlineAction->setText(QnCloudStatusPanel::tr("Cannot connect to %1").arg(QnAppInfo::cloudName()));
    offlineAction->setEnabled(false);

    offlineMenu->setWindowFlags(loggedInMenu->windowFlags() | Qt::BypassGraphicsProxyWidget);
    offlineMenu->addAction(offlineAction);
    offlineMenu->addSeparator();
    offlineMenu->addAction(q->action(QnActions::LogoutFromCloud));

    connect(qnCloudStatusWatcher, &QnCloudStatusWatcher::statusChanged, this,
        &QnCloudStatusPanelPrivate::updateUi);

#ifdef DIRECT_CLOUD_CONNECT
    systemsMenu = loggedInMenu->addMenu(QnCloudStatusPanel::tr("Connect to System..."));
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

    QPalette palette(originalPalette);

    switch (qnCloudStatusWatcher->status())
    {
        case QnCloudStatusWatcher::LoggedOut:
            q->setText(QnCloudStatusPanel::tr("Login to cloud..."));
            q->setIcon(QIcon());
            q->setMenu(nullptr);
            font.setWeight(QFont::Normal);
            palette.setColor(QPalette::ButtonText, palette.color(QPalette::Light));
            break;
        case QnCloudStatusWatcher::Online:
            q->setText(qnCloudStatusWatcher->cloudLogin());
            q->setIcon(loggedInIcon);
            q->setMenu(loggedInMenu);
            font.setWeight(QFont::Bold);
            break;
        case QnCloudStatusWatcher::Offline:
            q->setText(qnCloudStatusWatcher->cloudLogin());
            q->setIcon(offlineIcon);
            q->setMenu(offlineMenu);
            font.setWeight(QFont::Bold);
            break;
        default:
            NX_ASSERT(false, "Should never get here.");
            break;
    }
    q->setFont(font);
    q->adjustIconSize();
    q->setPalette(palette);
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
