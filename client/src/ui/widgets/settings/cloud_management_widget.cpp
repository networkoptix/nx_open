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
#include <common/common_module.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource_management/resource_properties.h>
#include <core/resource/param.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/user_resource.h>


QnCloudManagementWidget::QnCloudManagementWidget(QWidget *parent /* = NULL*/)
:
    QnAbstractPreferencesWidget(parent),
    QnWorkbenchContextAware(parent),
    ui(new Ui::CloudManagementWidget),
    m_connectionFactory(createConnectionFactory(), &destroyConnectionFactory),
    m_mserverReqHandle(-1),
    m_boundToCloud(false)
{
    ui->setupUi(this);

    connect(
        ui->toggleBindSystemButton, &QPushButton::clicked,
        this, &QnCloudManagementWidget::onToggleBindSystemClicked);
    connect(
        propertyDictionary, &QnResourcePropertyDictionary::asyncSaveDone,
        this, &QnCloudManagementWidget::onResourcePropertiesSaved);

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

void QnCloudManagementWidget::updateView(bool boundToCloud)
{
    m_boundToCloud = boundToCloud;
    if (m_boundToCloud)
    {
        ui->toggleBindSystemButton->setText(tr("Disconnect system from cloud"));
    }
    else
    {
        ui->toggleBindSystemButton->setText(tr("Connect system to cloud"));
    }
}

void QnCloudManagementWidget::onToggleBindSystemClicked()
{
    if(m_boundToCloud)
        doUnbindSystem();
    else
        doBindSystem();
}

void QnCloudManagementWidget::doBindSystem()
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

    ui->toggleBindSystemButton->setEnabled(false);
    //TODO #ak displaying progress

    m_cloudConnection = m_connectionFactory->createConnection(
        ui->cloudUserLineEdit->text().toStdString(),
        ui->cloudPasswordLineEdit->text().toStdString());

    nx::cdb::api::SystemRegistrationData sysRegistrationData;
    sysRegistrationData.name = qnCommon->localSystemName().toStdString();
    m_cloudConnection->systemManager()->bindSystem(
        sysRegistrationData,
        [this, serverConnection](
            nx::cdb::api::ResultCode result,
            nx::cdb::api::SystemData systemData)
        {
            QMetaObject::invokeMethod(
                this,
                "onBindDone",
                Qt::QueuedConnection,
                Q_ARG(
                    QnBindToCloudResponse,
                    QnBindToCloudResponse(result, std::move(systemData), serverConnection)));
        });
}

void QnCloudManagementWidget::doUnbindSystem()
{
    m_cloudConnection = m_connectionFactory->createConnection(
        ui->cloudUserLineEdit->text().toStdString(),
        ui->cloudPasswordLineEdit->text().toStdString());

    ui->toggleBindSystemButton->setEnabled(false);
    //TODO #ak displaying progress

    nx::cdb::api::SystemRegistrationData sysRegistrationData;
    m_cloudConnection->systemManager()->unbindSystem(
        qnResPool->getAdministrator()->getProperty(Qn::CLOUD_SYSTEM_ID).toStdString(),
        [this](nx::cdb::api::ResultCode result)
        {
            QMetaObject::invokeMethod(
                this,
                "onUnbindDone",
                Qt::QueuedConnection,
                Q_ARG(
                    int,
                    static_cast<int>(result)));
        });
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

void QnCloudManagementWidget::onBindDone(QnBindToCloudResponse response)
{
    if (response.resultCode != nx::cdb::api::ResultCode::ok)
    {
        QMessageBox::warning(
            this,   
            tr("Failed to connect system to cloud account"),
            QString::fromStdString(m_connectionFactory->toString(response.resultCode)));
        ui->toggleBindSystemButton->setEnabled(true);
        return;
    }

    m_mserverReqHandle = response.serverConnection->saveCloudSystemCredentials(
        response.systemData.id.toString(),
        QString::fromStdString(response.systemData.authKey),
        this,
        SLOT(onCloudCredentialsSaved(int, int)));
}

void QnCloudManagementWidget::onUnbindDone(int intResultCode)
{
    const auto resultCode = static_cast<nx::cdb::api::ResultCode>(intResultCode);
    if (resultCode != nx::cdb::api::ResultCode::ok)
    {
        QMessageBox::warning(
            this,
            tr("Failed to disconnect system from cloud account"),
            QString::fromStdString(m_connectionFactory->toString(resultCode)));
        ui->toggleBindSystemButton->setEnabled(true);
        return;
    }

    //TODO #ak removing properties from settings
    const auto admin = qnResPool->getAdministrator();

    admin->setProperty(Qn::CLOUD_SYSTEM_ID, QVariant());
    admin->setProperty(Qn::CLOUD_SYSTEM_AUTH_KEY, QVariant());
    m_mserverReqHandle = propertyDictionary->saveParamsAsync(admin->getId());
}

void QnCloudManagementWidget::onCloudCredentialsSaved(int status, int handle)
{
    cloudOperationCompleted(
        handle,
        status == 0,
        true,
        status == 0
            ? tr("Success")
            : tr("Failed to connect system to cloud account"),
        status == 0
            ? tr("System has been connected to the cloud account")
            : tr("Could not save info to DB"));
}

void QnCloudManagementWidget::onResourcePropertiesSaved(int handle, ec2::ErrorCode errorCode)
{
    cloudOperationCompleted(
        handle,
        errorCode == ec2::ErrorCode::ok,
        false,
        errorCode == ec2::ErrorCode::ok
            ? tr("Success")
            : tr("Failed to remove cloud information from DB"),
        errorCode == ec2::ErrorCode::ok
            ? tr("System has been disconnected from the cloud account")
            : ec2::toString(errorCode));
}

void QnCloudManagementWidget::cloudOperationCompleted(
    int requestHandle,
    bool ok,
    bool boundToCloud,
    const QString& messageTitle,
    const QString& messageText)
{
    if (requestHandle != m_mserverReqHandle)
        return;
    m_mserverReqHandle = -1;

    ui->toggleBindSystemButton->setEnabled(true);
    ui->cloudPasswordLineEdit->clear();

    if (!ok)
    {
        QMessageBox::warning(
            this,
            messageTitle,
            messageText);
        return;
    }

    QMessageBox::information(
        this,
        messageTitle,
        messageText);
    updateView(boundToCloud);
}
