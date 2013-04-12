#include "system_health_popup_widget.h"
#include "ui_system_health_popup_widget.h"

#include <core/resource/resource.h>
#include <core/resource/user_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource_managment/resource_pool.h>

#include <ui/actions/actions.h>
#include <ui/actions/action_manager.h>
#include <ui/actions/action_parameters.h>
#include <ui/common/resource_name.h>
#include <ui/style/globals.h>
#include <ui/style/resource_icon_cache.h>
#include <ui/workbench/workbench_context.h>

namespace {
    static bool localResource_less_than(const QnResourcePtr &u1, const QnResourcePtr &u2)
    {
        return QString::compare(u1->getName(), u2->getName()) < 0;
    }
}

QnSystemHealthPopupWidget::QnSystemHealthPopupWidget(QWidget *parent) :
    base_type(parent),
    QnWorkbenchContextAware(parent),
    ui(new Ui::QnSystemHealthPopupWidget),
    m_messageType(QnSystemHealth::NotDefined)
{
    ui->setupUi(this);

    setBorderColor(qnGlobals->popupFrameSystem());
    connect(ui->fixButton,      SIGNAL(clicked()), this, SLOT(at_fixButton_clicked()));
    connect(ui->postponeButton, SIGNAL(clicked()), this, SLOT(at_postponeButton_clicked()));
}

QnSystemHealthPopupWidget::~QnSystemHealthPopupWidget()
{
}

bool QnSystemHealthPopupWidget::showSystemHealthMessage(QnSystemHealth::MessageType message, const QnResourceList &resources) {

    QLatin1String htmlTemplate("<html><head/><body><p>%1 <a href=\"%2\">"\
                               "<span style=\"text-decoration: underline; \">"\
                               "%3</span></a></p></body></html>");

    m_messageType = message;
    if (message == QnSystemHealth::NotDefined)
        return false;
    ui->messageLabel->setText(QnSystemHealth::toString(m_messageType));
    ui->resourcesListWidget->setVisible(!resources.isEmpty());
    ui->fixButton->setVisible(resources.isEmpty());

    QnResourceList localResources = resources;
    qSort(localResources.begin(), localResources.end(), localResource_less_than);
    foreach (const QnResourcePtr &resource, localResources) {
        QWidget* labelsWidget = new QWidget(ui->resourcesListWidget);
        QHBoxLayout* layout = new QHBoxLayout();
        labelsWidget->setLayout(layout);
        layout->setContentsMargins(0, 0, 0, 0);
        ui->resourcesListLayout->addWidget(labelsWidget);

        QLabel* labelIcon = new QLabel(labelsWidget);
        labelIcon->setPixmap(qnResIconCache->icon(resource->flags(), resource->getStatus()).pixmap(18, 18));
        layout->addWidget(labelIcon);

        QLabel* labelName = new QLabel(labelsWidget);
        labelName->setText( QString(htmlTemplate)
                            .arg(getResourceName(resource))
                            .arg(QString::number(resource->getId().toInt()))
                            .arg(tr("( Fix... )"))
                           );
        layout->addWidget(labelName);

        switch (message) {
        case QnSystemHealth::UsersEmailIsEmpty:
            connect(labelName, SIGNAL(linkActivated(QString)), this, SLOT(at_fixUserLabel_linkActivated(QString)));
            break;
        case QnSystemHealth::StoragesAreFull:
        case QnSystemHealth::StoragesNotConfigured:
            connect(labelName, SIGNAL(linkActivated(QString)), this, SLOT(at_fixStoragesLabel_linkActivated(QString)));
            break;
        default:
            break;
        }
        QSpacerItem* horizontalSpacer = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);
        layout->addItem(horizontalSpacer);
    }

    return true;
}

void QnSystemHealthPopupWidget::at_fixButton_clicked() {
    switch (m_messageType) {
    case QnSystemHealth::EmailIsEmpty:
        {
            QnActionParameters params(context()->user());
            params.setFocusElement(QLatin1String("email"));
            menu()->trigger(Qn::UserSettingsAction, params);
        }
        break;
    case QnSystemHealth::NoLicenses:
        menu()->trigger(Qn::GetMoreLicensesAction);
        break;
    case QnSystemHealth::SmtpIsNotSet:
    case QnSystemHealth::EmailSendError:
        menu()->trigger(Qn::OpenServerSettingsAction);
        break;
    case QnSystemHealth::ConnectionLost:
        menu()->trigger(Qn::ConnectToServerAction);
        break;
    default:
        break;
    }

    emit closed(m_messageType, ui->ignoreCheckBox->isChecked());
    hide();
}

void QnSystemHealthPopupWidget::at_fixUserLabel_linkActivated(const QString &anchor) {
    int id = anchor.toInt();
    QnUserResourcePtr user = qnResPool->getResourceById(id).dynamicCast<QnUserResource>();
    if (!user)
        return;
    QnActionParameters params(user);
    params.setFocusElement(QLatin1String("email"));
    menu()->trigger(Qn::UserSettingsAction, params);

    QWidget* p = dynamic_cast<QWidget *>(sender()->parent());
    if (!p)
        return;

    ui->resourcesListLayout->removeWidget(p);
    p->hide();
    if (ui->resourcesListLayout->count() == 0)
        emit closed(m_messageType, ui->ignoreCheckBox->isChecked());
}

void QnSystemHealthPopupWidget::at_fixStoragesLabel_linkActivated(const QString &anchor) {
    int id = anchor.toInt();
    QnMediaServerResourcePtr server = qnResPool->getResourceById(id).dynamicCast<QnMediaServerResource>();
    if (!server)
        return;
    QnActionParameters params(server);
    menu()->trigger(Qn::ServerSettingsAction, params);

    QWidget* p = dynamic_cast<QWidget *>(sender()->parent());
    if (!p)
        return;

    ui->resourcesListLayout->removeWidget(p);
    p->hide();
    if (ui->resourcesListLayout->count() == 0)
        emit closed(m_messageType, ui->ignoreCheckBox->isChecked());
}

void QnSystemHealthPopupWidget::at_postponeButton_clicked() {
    emit closed(m_messageType, ui->ignoreCheckBox->isChecked());
    hide();
}
