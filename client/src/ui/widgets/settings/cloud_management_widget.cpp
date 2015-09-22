/**********************************************************
* Sep 17, 2015
* a.kolesnikov
***********************************************************/

#include "cloud_management_widget.h"
#include "ui_cloud_management_widget.h"

#include <QtCore/QMetaObject>
#include <QtCore/QSortFilterProxyModel>
#include <QtWidgets/QMessageBox>

#include <api/app_server_connection.h>
#include <api/media_server_connection.h>
//#include <api/global_settings.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/param.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/user_resource.h>


QnCloudManagementWidget::QnCloudManagementWidget(QWidget *parent /* = NULL*/)
:
    QnAbstractPreferencesWidget(parent),
    QnWorkbenchContextAware(parent),
    ui(new Ui::CloudManagementWidget),
    m_connectionFactory(createConnectionFactory(), &destroyConnectionFactory),
    m_mserverReqHandle(-1)
{
    ui->setupUi(this);

    connect(
        ui->bindSystemToCloudButton, &QPushButton::clicked,
        this, &QnCloudManagementWidget::onBindSystemClicked);
    connect(
        ui->unbindSystemButton, &QPushButton::clicked,
        this, &QnCloudManagementWidget::onUnbindSystemClicked);

    connect(
        qnResPool, &QnResourcePool::resourceChanged,
        this, &QnCloudManagementWidget::onUserChanged);
    //setHelpTopic(this, Qn::Administration_CloudManagement_Help);

    onUserChanged(qnResPool->getAdministrator());
}

QnCloudManagementWidget::~QnCloudManagementWidget() {
}

void QnCloudManagementWidget::updateFromSettings() {
    //QnGlobalSettings *settings = QnGlobalSettings::instance();
    //TODO #ak
}

void QnCloudManagementWidget::submitToSettings() {
    //TODO #ak
}

bool QnCloudManagementWidget::hasChanges() const {
    //TODO #ak
    return false;
}

void QnCloudManagementWidget::updateView(bool bindedToCloud)
{
    ui->stackedWidget->setCurrentIndex(bindedToCloud ? 1 : 0);
}

void QnCloudManagementWidget::onBindSystemClicked()
{
    QnMediaServerConnectionPtr serverConnection;
    for (const QnMediaServerResourcePtr server : qnResPool->getAllServers())
    {
        if (server->getStatus() != Qn::Online)
            continue;

        if (!(server->getServerFlags() & Qn::SF_HasPublicIP))
            continue;

        serverConnection = server->apiConnection();
        break;
    }

    if (!serverConnection)
    {
        QMessageBox::warning(
            this,
            tr("Network Error"),
            tr("Failed. None of your servers is connected to the Internet."));
        return;
    }

    m_cloudConnection = m_connectionFactory->createConnection(
        ui->cloudUserLineEdit->text().toStdString(),
        ui->cloudPasswordLineEdit->text().toStdString());

    using namespace std::placeholders;
    nx::cdb::api::SystemRegistrationData sysRegistrationData;
    m_cloudConnection->systemManager()->bindSystem(
        sysRegistrationData,
        [this, serverConnection](
            nx::cdb::api::ResultCode result,
            nx::cdb::api::SystemData systemData)
        {
            QMetaObject::invokeMethod(
                this,
                "onBindingDone",
                Qt::QueuedConnection,
                Q_ARG(
                    QnBindToCloudResponse,
                    QnBindToCloudResponse(result, std::move(systemData), serverConnection)));
        });
}

void QnCloudManagementWidget::onUnbindSystemClicked()
{
    //TODO #ak
}

void QnCloudManagementWidget::onUserChanged(QnResourcePtr resource)
{
    const auto user = resource.dynamicCast<QnUserResource>();
    if (!user || !user->isAdmin())
        return;

    updateView(
        !user->getProperty(Qn::CLOUD_SYSTEM_ID).isEmpty() &&
        !user->getProperty(Qn::CLOUD_SYSTEM_AUTH_KEY).isEmpty());
}

void QnCloudManagementWidget::onBindingDone(QnBindToCloudResponse response)
{
    if (response.resultCode != nx::cdb::api::ResultCode::ok)
    {
        QMessageBox::warning(
            this,   
            tr("Failed to bind system to cloud account"),
            tr("TODO error text"));
        return;
    }

    response.serverConnection->saveCloudSystemCredentials(
        response.systemData.id.toString(),
        QString::fromStdString(response.systemData.authKey),
        this,
        SLOT(onCloudCredentialsSaved(int, int)));
}

void QnCloudManagementWidget::onCloudCredentialsSaved(int status, int handle)
{
    if (handle != m_mserverReqHandle)
        return;
    m_mserverReqHandle = -1;

    if (status != 0)
    {
        QMessageBox::warning(
            this,
            tr("Failed to bind system to cloud account"),
            tr("Could not save info to DB"));
        return;
    }

    updateView(true);
}
