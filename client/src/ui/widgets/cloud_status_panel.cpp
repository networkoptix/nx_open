#include "cloud_status_panel.h"

#include <QtWidgets/QPushButton>
#include <QtWidgets/QMenu>

#include <common/common_module.h>
#include <watchers/cloud_status_watcher.h>

class QnCloudStatusPanelPrivate : public QObject
{
    QnCloudStatusPanel *q_ptr;
    Q_DECLARE_PUBLIC(QnCloudStatusPanel)

public:
    QnCloudStatusPanelPrivate(QnCloudStatusPanel *parent);

    void updateUi();
    void at_clicked();

public:
    QMenu *cloudMenu;
};

QnCloudStatusPanel::QnCloudStatusPanel(QWidget *parent)
    : base_type(parent)
    , QnWorkbenchContextAware(parent)
    , d_ptr(new QnCloudStatusPanelPrivate(this))
{
    Q_D(QnCloudStatusPanel);

    connect(this, &QnCloudStatusPanel::clicked, d, &QnCloudStatusPanelPrivate::at_clicked);

    setFlat(true);

    d->updateUi();
}

QnCloudStatusPanel::~QnCloudStatusPanel()
{
}

QnCloudStatusPanelPrivate::QnCloudStatusPanelPrivate(QnCloudStatusPanel *parent)
    : QObject(parent)
    , q_ptr(parent)
    , cloudMenu(new QMenu(parent))
{
    Q_Q(QnCloudStatusPanel);

    cloudMenu->addAction(q->action(Qn::OpenCloudMainUrl));
    cloudMenu->addSeparator();
    cloudMenu->addAction(q->action(Qn::OpenCloudManagementUrl));
    cloudMenu->addAction(q->action(Qn::LogoutFromCloud));

    QnCloudStatusWatcher *cloudStatusWatcher = qnCommon->instance<QnCloudStatusWatcher>();
    connect(cloudStatusWatcher, &QnCloudStatusWatcher::statusChanged, this, &QnCloudStatusPanelPrivate::updateUi);
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

    q->setText(cloudStatusWatcher->cloudLogin());
    // q->setIcon(); TODO: #dklychkov
    q->setMenu(cloudMenu);
}

void QnCloudStatusPanelPrivate::at_clicked()
{
    Q_Q(QnCloudStatusPanel);

    if (q->QPushButton::menu())
        return;

    q->action(Qn::LoginToCLoud)->trigger();
}
