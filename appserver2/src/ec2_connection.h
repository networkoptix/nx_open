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
    // TODO: #2.4 remove Ec2 prefix to avoid ec2::Ec2DirectConnection
    class Ec2DirectConnection
    :
        public BaseEc2Connection<ServerQueryProcessorAccess>
    {
    public:
        Ec2DirectConnection(
            const Ec2DirectConnectionFactory* connectionFactory,
            ServerQueryProcessorAccess* queryProcessor,
            const QnConnectionInfo& connectionInfo,
            const QUrl& dbUrl);
        virtual ~Ec2DirectConnection();

        //!Implementation of ec2::AbstractECConnection::connectionInfo
        virtual QnConnectionInfo connectionInfo() const override;
        //!Implementation of ec2::AbstractECConnection::authInfo
        virtual QString authInfo() const override;

        bool initialized() const;

        Ec2StaticticsReporter* getStaticticsReporter();

        virtual Timestamp getTransactionLogTime() const override;
        virtual void setTransactionLogTime(Timestamp value) override;

    private:
        const QnConnectionInfo m_connectionInfo;
        bool m_isInitialized;
        std::unique_ptr<Ec2StaticticsReporter> m_staticticsReporter;
    };
    typedef std::shared_ptr<Ec2DirectConnection> Ec2DirectConnectionPtr;
}

#endif  //EC2_CONNECTION_H
