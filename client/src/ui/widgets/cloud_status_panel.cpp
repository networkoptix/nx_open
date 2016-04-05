#include "cloud_status_panel.h"

#include <QtWidgets/QMenu>

#include <common/common_module.h>
#include <watchers/cloud_status_watcher.h>
#include <ui/style/custom_style.h>
#include <ui/style/helper.h>

class QnCloudStatusPanelPrivate : public QObject
{
    QnCloudStatusPanel *q_ptr;
    Q_DECLARE_PUBLIC(QnCloudStatusPanel)

public:
    QnCloudStatusPanelPrivate(QnCloudStatusPanel *parent);

    void updateUi();
    void updateSystems();
    void at_clicked();

public:
    QMenu *cloudMenu;
    QMenu *systemsMenu;
    QPalette originalPalette;
};

QnCloudStatusPanel::QnCloudStatusPanel(QWidget *parent)
    : base_type(parent)
    , QnWorkbenchContextAware(parent)
    , d_ptr(new QnCloudStatusPanelPrivate(this))
{
    Q_D(QnCloudStatusPanel);

    connect(this, &QnCloudStatusPanel::clicked, d, &QnCloudStatusPanelPrivate::at_clicked);
    setPopupMode(QToolButton::InstantPopup);

    setProperty(style::Properties::kDontPolishFontProperty, true);
    QFont font = qApp->font();
    font.setPixelSize(font.pixelSize() - 1);
    setFont(font);

    d->updateUi();
}

QnCloudStatusPanel::~QnCloudStatusPanel()
{
}

QnCloudStatusPanelPrivate::QnCloudStatusPanelPrivate(QnCloudStatusPanel *parent)
    : QObject(parent)
    , q_ptr(parent)
    , cloudMenu(new QMenu(parent))
    , systemsMenu(nullptr)
{
    Q_Q(QnCloudStatusPanel);

    cloudMenu->addAction(q->action(QnActions::OpenCloudMainUrl));
    systemsMenu = cloudMenu->addMenu(tr("Connect to System..."));
    cloudMenu->addSeparator();
    cloudMenu->addAction(q->action(QnActions::OpenCloudManagementUrl));
    cloudMenu->addAction(q->action(QnActions::LogoutFromCloud));

    QnCloudStatusWatcher *cloudStatusWatcher = qnCommon->instance<QnCloudStatusWatcher>();
    connect(cloudStatusWatcher,     &QnCloudStatusWatcher::statusChanged,           this,   &QnCloudStatusPanelPrivate::updateUi);
    //TODO: #dklychkov Uncomment when cloud login is implemented
//    connect(cloudStatusWatcher,     &QnCloudStatusWatcher::cloudSystemsChanged,     this,   &QnCloudStatusPanelPrivate::updateSystems);

    updateSystems();
}

void QnCloudStatusPanelPrivate::updateUi()
{
    Q_Q(QnCloudStatusPanel);

    QnCloudStatusWatcher *cloudStatusWatcher = qnCommon->instance<QnCloudStatusWatcher>();

    const bool isLogged = (cloudStatusWatcher->status() == QnCloudStatusWatcher::Online);
    if (!isLogged)
    {
        q->setText(tr("Login to cloud..."));
        q->setIcon(QIcon());
        q->setMenu(nullptr);
        return;
    }

    // TODO: #dklychkov display status
    if (isLogged)
        q->setPalette(originalPalette);
    else
        setWarningStyle(q);

    q->setText(cloudStatusWatcher->cloudLogin());
    // q->setIcon(); TODO: #dklychkov
    q->setMenu(cloudMenu);
}

void QnCloudStatusPanelPrivate::updateSystems()
{
    QnCloudStatusWatcher *cloudStatusWatcher = qnCommon->instance<QnCloudStatusWatcher>();
    const QnCloudSystemList &systems = cloudStatusWatcher->cloudSystems();

    systemsMenu->clear();
    for (const QnCloudSystem &system: systems)
    {
        QAction *action = systemsMenu->addAction(system.name);
        QUrl url;
        //TODO: #dklychkov Prepare URL
        action->setData(url);
    }

    systemsMenu->menuAction()->setVisible(!systemsMenu->actions().isEmpty());
}

void QnCloudStatusPanelPrivate::at_clicked()
{
    Q_Q(QnCloudStatusPanel);

    if (q->base_type::menu())
    {
        qnCommon->instance<QnCloudStatusWatcher>()->updateSystems();
        return;
    }

    q->action(QnActions::LoginToCLoud)->trigger();
}
