#include "cloud_status_panel.h"

#include <QtWidgets/QPushButton>
#include <QtWidgets/QMenu>

#include <common/common_module.h>
#include <watchers/cloud_status_watcher.h>
#include <ui/style/custom_style.h>

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

QnCloudStatusPanel::QnCloudStatusPanel(QnWorkbenchContext *context, QWidget *parent)
    : base_type(parent)
    , QnWorkbenchContextAware(context)
    , d_ptr(new QnCloudStatusPanelPrivate(this))
{
    Q_D(QnCloudStatusPanel);

    connect(this, &QnCloudStatusPanel::clicked, d, &QnCloudStatusPanelPrivate::at_clicked);

    setFlat(true);

    d->originalPalette = this->palette();
    d->originalPalette.setColor(QPalette::Window, qApp->palette().color(QPalette::Window));
    d->cloudMenu->setPalette(d->originalPalette);
    d->originalPalette.setColor(QPalette::Window, Qt::transparent);
    d->originalPalette.setColor(QPalette::Button, Qt::transparent);
    d->originalPalette.setColor(QPalette::Highlight, Qt::transparent);
    setPalette(d->originalPalette);

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

    if (cloudStatusWatcher->status() == QnCloudStatusWatcher::LoggedOut)
    {
        q->setText(tr("Login to cloud..."));
        q->setIcon(QIcon());
        q->setMenu(nullptr);
        return;
    }

    // TODO: #dklychkov display status
    if (cloudStatusWatcher->status() == QnCloudStatusWatcher::Online)
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

    if (q->QPushButton::menu())
    {
        qnCommon->instance<QnCloudStatusWatcher>()->updateSystems();
        return;
    }

    q->action(QnActions::LoginToCLoud)->trigger();
}
