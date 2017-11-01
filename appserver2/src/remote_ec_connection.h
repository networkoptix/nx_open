/**********************************************************
* 30 jan 2014
* a.kolesnikov
***********************************************************/

#ifndef REMOTE_EC_CONNECTION_H
#define REMOTE_EC_CONNECTION_H

#include <memory>

#include <nx_ec/ec_api.h>

#include "base_ec2_connection.h"
#include "client_query_processor.h"
#include "fixed_url_client_query_processor.h"


namespace ec2
{
    class RemoteEC2Connection
    :
        public BaseEc2Connection<FixedUrlClientQueryProcessor>
    {
    typedef BaseEc2Connection<FixedUrlClientQueryProcessor> base_type;

    public:
        RemoteEC2Connection(
            const AbstractECConnectionFactory* connectionFactory,
            const QnUuid& remotePeerId,
            const FixedUrlClientQueryProcessorPtr& queryProcessor,
            const QnConnectionInfo& connectionInfo );
        virtual ~RemoteEC2Connection();

        virtual QnConnectionInfo connectionInfo() const override;
        virtual void updateConnectionUrl(const nx::utils::Url& url) override;

        virtual void startReceivingNotifications() override;
        virtual void stopReceivingNotifications() override;

        virtual Timestamp getTransactionLogTime() const override;
        virtual void setTransactionLogTime(Timestamp value) override;

    private:
        FixedUrlClientQueryProcessorPtr m_queryProcessor;
        QnConnectionInfo m_connectionInfo;
        QnUuid m_remotePeerId;
    };
    typedef std::shared_ptr<RemoteEC2Connection> RemoteEC2ConnectionPtr;
}

#endif  //REMOTE_EC_CONNECTION_H
