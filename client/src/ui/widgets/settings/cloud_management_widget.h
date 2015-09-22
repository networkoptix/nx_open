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
#include <utils/common/uuid.h>


namespace Ui {
    class CloudManagementWidget;
}

class QnBindToCloudResponse
{
public:
    nx::cdb::api::ResultCode resultCode;
    nx::cdb::api::SystemData systemData;
    QnMediaServerConnectionPtr serverConnection;

    QnBindToCloudResponse() {}
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

    virtual void updateFromSettings() override;
    virtual void submitToSettings() override;

    virtual bool hasChanges() const override;

private:
    QScopedPointer<Ui::CloudManagementWidget> ui;
    std::unique_ptr<
        nx::cdb::api::ConnectionFactory,
        decltype(&destroyConnectionFactory)> m_connectionFactory;
    std::unique_ptr<nx::cdb::api::Connection> m_cloudConnection;
    int m_mserverReqHandle;

    void updateView(bool bindedToCloud);

private slots:
    void onBindSystemClicked();
    void onUnbindSystemClicked();
    void onUserChanged(QnResourcePtr resource);
    
    void onBindingDone(QnBindToCloudResponse response);
    void onCloudCredentialsSaved(int status, int handle);
};

#endif // QN_TIME_SERVER_SELECTION_WIDGET_H
