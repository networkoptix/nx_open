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
#include "managers/impl/stored_file_manager_impl.h"


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
            const QString& dbFileName);

        //!Implementation of ec2::AbstractECConnection::connectionInfo
        virtual QnConnectionInfo connectionInfo() const override;
        //!Implementation of ec2::AbstractECConnection::startReceivingNotifications
        virtual void startReceivingNotifications( bool fullSyncRequired) override;

    private:
        StoredFileManagerImpl m_storedFileManagerImpl;
		std::unique_ptr<QnDbManager> m_dbManager;   //TODO: #ak not sure this is right place for QnDbManager instance
        const QnConnectionInfo m_connectionInfo;
    };
    typedef std::shared_ptr<Ec2DirectConnection> Ec2DirectConnectionPtr;
}

#endif  //EC2_CONNECTION_H
