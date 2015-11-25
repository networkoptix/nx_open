/**********************************************************
* Sep 17, 2015
* a.kolesnikov
***********************************************************/

#ifndef QN_CLOUD_MANAGEMENT_WIDGET_H
#define QN_CLOUD_MANAGEMENT_WIDGET_H

#include <QtWidgets/QWidget>

#include <cdb/connection.h>

#include <api/api_fwd.h>
#include <core/resource/resource_fwd.h>
#include <nx_ec/ec_api_fwd.h>
#include <ui/widgets/settings/abstract_preferences_widget.h>
#include <ui/workbench/workbench_context_aware.h>
#include <nx/tool/uuid.h>


namespace Ui {
    class CloudManagementWidget;
}

class QnBindToCloudResponse
{
public:
    nx::cdb::api::ResultCode resultCode;
    nx::cdb::api::SystemData systemData;
    QnMediaServerConnectionPtr serverConnection;

    QnBindToCloudResponse()
    :
        resultCode(nx::cdb::api::ResultCode::ok)
    {
    }

    QnBindToCloudResponse(
        nx::cdb::api::ResultCode _resultCode,
        nx::cdb::api::SystemData _systemData,
        QnMediaServerConnectionPtr _serverConnection)
    :
        resultCode(_resultCode),
        systemData(std::move(_systemData)),
        serverConnection(std::move(_serverConnection))
    {
    }
};

Q_DECLARE_METATYPE(QnBindToCloudResponse)

class QnCloudManagementWidget: public QnAbstractPreferencesWidget, public QnWorkbenchContextAware {
    Q_OBJECT
public:
    QnCloudManagementWidget(QWidget *parent = NULL);
    virtual ~QnCloudManagementWidget();

    virtual void loadDataToUi() override;
    virtual void applyChanges() override;

    virtual bool hasChanges() const override;

private:
    QScopedPointer<Ui::CloudManagementWidget> ui;
    std::unique_ptr<
        nx::cdb::api::ConnectionFactory,
        decltype(&destroyConnectionFactory)> m_connectionFactory;
    std::unique_ptr<nx::cdb::api::Connection> m_cloudConnection;
    int m_mserverReqHandle;
    bool m_boundToCloud;

    void updateView(bool boundToCloud);

    void cloudOperationCompleted(
        int requestHandle,
        bool ok,
        bool boundToCloud,
        const QString& messageTitle,
        const QString& messageText);

private slots:
    void onToggleBindSystemClicked();
    void doBindSystem();
    void doUnbindSystem();
    void onUserChanged(QnResourcePtr resource);
    
    void onBindDone(QnBindToCloudResponse response);
    void onUnbindDone(int resultCode);
    void onCloudCredentialsSaved(int status, int handle);
    void onResourcePropertiesSaved(int recId, ec2::ErrorCode errorCode);
};

#endif // QN_TIME_SERVER_SELECTION_WIDGET_H
