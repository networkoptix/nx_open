/**********************************************************
* 21 jan 2014
* a.kolesnikov
***********************************************************/

#ifndef EC2_CONNECTION_H
#define EC2_CONNECTION_H

#include <memory>

#include "base_ec2_connection.h"
#include "database/db_manager.h"
#include "server_query_processor.h"
#include "managers/impl/license_manager_impl.h"
#include "managers/impl/email_manager_impl.h"


namespace ec2
{
    class Ec2DirectConnection
    :
        public BaseEc2Connection<ServerQueryProcessor>
    {
    public:
        Ec2DirectConnection(
            ServerQueryProcessor* queryProcessor,
            const ResourceContext& resCtx,
            const QnConnectionInfo& connectionInfo,
            const QUrl& dbUrl);
        virtual ~Ec2DirectConnection();

        //!Implementation of ec2::AbstractECConnection::connectionInfo
        virtual QnConnectionInfo connectionInfo() const override;
        //!Implementation of ec2::AbstractECConnection::startReceivingNotifications
        virtual void startReceivingNotifications() override;

    private:
        LicenseManagerImpl m_licenseManagerImpl;
        EmailManagerImpl m_emailManagerImpl;
		std::unique_ptr<QnDbManager> m_dbManager;   //TODO: #ak not sure this is right place for QnDbManager instance
        std::unique_ptr<QnAuxManager> m_auxManager;
        std::unique_ptr<QnTransactionLog> m_transactionLog;
        const QnConnectionInfo m_connectionInfo;
    };
    typedef std::shared_ptr<Ec2DirectConnection> Ec2DirectConnectionPtr;
}

#endif  //EC2_CONNECTION_H
