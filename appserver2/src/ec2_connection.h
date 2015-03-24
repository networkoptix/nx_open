/**********************************************************
* 21 jan 2014
* a.kolesnikov
***********************************************************/

#ifndef EC2_CONNECTION_H
#define EC2_CONNECTION_H

#include <memory>

#include "base_ec2_connection.h"
#include "ec2_statictics_reporter.h"
#include "database/db_manager.h"
#include "server_query_processor.h"

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
        //!Implementation of ec2::AbstractECConnection::authInfo
        virtual QString authInfo() const override;

        bool initialized() const;

        Ec2StaticticsReporter* getStaticticsReporter();
    
    private:
        std::unique_ptr<QnTransactionLog> m_transactionLog;
        const QnConnectionInfo m_connectionInfo;
        bool m_isInitialized;
        std::unique_ptr<Ec2StaticticsReporter> m_staticticsReporter;
    };
    typedef std::shared_ptr<Ec2DirectConnection> Ec2DirectConnectionPtr;
}

#endif  //EC2_CONNECTION_H
