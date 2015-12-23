#include "cloud_status_panel.h"

#include <QtWidgets/QPushButton>
#include <QtWidgets/QMenu>

#include <common/common_module.h>
#include <watchers/cloud_status_watcher.h>
#include <ui/style/warning_style.h>

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
    if (cloudStatusWatcher->status() == QnCloudStatusWatcher::Online)
        q->setPalette(originalPalette);
    else
        setWarningStyle(q);

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
