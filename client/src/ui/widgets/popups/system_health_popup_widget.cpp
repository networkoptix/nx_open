#include "system_health_popup_widget.h"
#include "ui_system_health_popup_widget.h"

#include <core/resource/resource.h>
#include <core/resource/user_resource.h>
#include <core/resource_managment/resource_pool.h>

#include <ui/actions/actions.h>
#include <ui/actions/action_manager.h>
#include <ui/actions/action_parameters.h>
#include <ui/style/globals.h>
#include <ui/style/resource_icon_cache.h>
#include <ui/workbench/workbench_context.h>

namespace {
    static bool userResource_less_than(const QnUserResourcePtr &u1, const QnUserResourcePtr &u2)
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

    setBorderColor(qnGlobals->popupFrameSystem()); //TODO: #GDM skin color
    connect(ui->fixButton,      SIGNAL(clicked()), this, SLOT(at_fixButton_clicked()));
    connect(ui->postponeButton, SIGNAL(clicked()), this, SLOT(at_postponeButton_clicked()));
}

QnSystemHealthPopupWidget::~QnSystemHealthPopupWidget()
{
}

bool QnSystemHealthPopupWidget::showSystemHealthMessage(QnSystemHealth::MessageType message, const QnUserResourceList &users) {

    QLatin1String htmlTemplate("<html><head/><body><p>%1 <a href=\"%2\">"\
                               "<span style=\"text-decoration: underline; \">"\
                               "%3</span></a></p></body></html>");

    m_messageType = message;
    if (message == QnSystemHealth::NotDefined)
        return false;
    ui->messageLabel->setText(QnSystemHealth::toString(m_messageType));
    ui->userListWidget->setVisible(!users.isEmpty());
    ui->fixButton->setVisible(users.isEmpty());

    QnUserResourceList localUsers = users;
    qSort(localUsers.begin(), localUsers.end(), userResource_less_than);
    foreach (const QnUserResourcePtr &user, localUsers) {
        QHBoxLayout* layout = new QHBoxLayout();
        ui->userListLayout->addLayout(layout);

        QLabel* labelIcon = new QLabel(ui->userListWidget);
        labelIcon->setPixmap(qnResIconCache->icon(user->flags(), user->getStatus()).pixmap(18, 18));
        layout->addWidget(labelIcon);

        QLabel* labelName = new QLabel(ui->userListWidget);
        labelName->setText( QString(htmlTemplate)
                            .arg(user->getName())
                            .arg(QString::number(user->getId().toInt()))
                            .arg(tr("( Fix... )"))
                           );
        layout->addWidget(labelName);

        connect(labelName, SIGNAL(linkActivated(QString)), this, SLOT(at_fixUserLabel_linkActivated(QString)));

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
    case QnSystemHealth::StoragesAreFull:
    case QnSystemHealth::StoragesNotConfigured:
        /*
         menu()->trigger(Qn::ServerSettingsAction,
            QnActionParameters(server));
        */
        //TODO: #GDM read resource list and open dialog clicking on "Fix" url.
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
}

void QnSystemHealthPopupWidget::at_postponeButton_clicked() {
    emit closed(m_messageType, ui->ignoreCheckBox->isChecked());
    hide();
}
