#include "cloud_status_panel.h"

#include <QtWidgets/QMenu>

#include <watchers/cloud_status_watcher.h>
#include <ui/style/custom_style.h>
#include <ui/style/helper.h>
#include <ui/style/skin.h>

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
    QMenu* cloudMenu;
#ifdef DIRECT_CLOUD_CONNECT
    QMenu* systemsMenu;
#endif
    QPalette originalPalette;
    QIcon onlineIcon;
    QIcon offlineIcon;
};

QnCloudStatusPanel::QnCloudStatusPanel(QWidget* parent):
    base_type(parent),
    QnWorkbenchContextAware(parent),
    d_ptr(new QnCloudStatusPanelPrivate(this))
{
    Q_D(QnCloudStatusPanel);

    setProperty(style::Properties::kDontPolishFontProperty, true);
    QFont font = qApp->font();
    font.setPixelSize(font.pixelSize() - 1);
    setFont(font);

    setPopupMode(QToolButton::InstantPopup);
    setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    setIcon(d->offlineIcon);
    adjustIconSize();

    connect(this, &QnCloudStatusPanel::justPressed, qnCloudStatusWatcher,
        &QnCloudStatusWatcher::updateSystems);

    d->updateUi();
}

QnCloudStatusPanel::~QnCloudStatusPanel()
{
}

QnCloudStatusPanelPrivate::QnCloudStatusPanelPrivate(QnCloudStatusPanel* parent):
    QObject(parent),
    q_ptr(parent),
    cloudMenu(new QMenu(parent)),
#ifdef DIRECT_CLOUD_CONNECT
    systemsMenu(nullptr),
#endif
    onlineIcon(qnSkin->icon("titlebar/cloud_logged.png")),
    offlineIcon(qnSkin->icon("titlebar/cloud_not_logged.png"))
{
    Q_Q(QnCloudStatusPanel);

    cloudMenu->addAction(q->action(QnActions::OpenCloudMainUrl));
    cloudMenu->setWindowFlags(cloudMenu->windowFlags() | Qt::BypassGraphicsProxyWidget);

    cloudMenu->addSeparator();
    cloudMenu->addAction(q->action(QnActions::OpenCloudManagementUrl));
    cloudMenu->addAction(q->action(QnActions::LogoutFromCloud));

    connect(qnCloudStatusWatcher, &QnCloudStatusWatcher::statusChanged, this, &QnCloudStatusPanelPrivate::updateUi);

#ifdef DIRECT_CLOUD_CONNECT
    systemsMenu = cloudMenu->addMenu(QnCloudStatusPanel::tr("Connect to System..."));
    connect(qnCloudStatusWatcher, &QnCloudStatusWatcher::cloudSystemsChanged, this, &QnCloudStatusPanelPrivate::updateSystems);
    updateSystems();
#endif
}

void QnCloudStatusPanelPrivate::updateUi()
{
    Q_Q(QnCloudStatusPanel);

    if (qnCloudStatusWatcher->status() == QnCloudStatusWatcher::LoggedOut)
    {
        q->setText(QnCloudStatusPanel::tr("Login to cloud..."));
        q->setIcon(offlineIcon);
        q->setMenu(nullptr);
        connect(q, &QnCloudStatusPanel::clicked, q->action(QnActions::LoginToCloud), &QAction::trigger);
        return;
    }

    // TODO: #dklychkov display status
    if (qnCloudStatusWatcher->status() == QnCloudStatusWatcher::Online)
        q->setPalette(originalPalette);
    else
        setWarningStyle(q);

    q->setText(qnCloudStatusWatcher->cloudLogin());
    q->setIcon(onlineIcon);
    q->setMenu(cloudMenu);
    disconnect(q, &QnCloudStatusPanel::clicked, q->action(QnActions::LoginToCloud), &QAction::trigger);
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
