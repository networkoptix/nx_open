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
            const QnConnectionInfo& connectionInfo );

        virtual QnConnectionInfo connectionInfo() const;

    private:
		std::unique_ptr<QnDbManager> m_dbManager;
        const QnConnectionInfo m_connectionInfo;
    };
}

#endif  //EC2_CONNECTION_H
