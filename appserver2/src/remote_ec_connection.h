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
    class QnTransactionMessageBus;

    class RemoteEC2Connection
    :
        public BaseEc2Connection<FixedUrlClientQueryProcessor>
    {
    public:
        RemoteEC2Connection(
            const FixedUrlClientQueryProcessorPtr& queryProcessor,
            const ResourceContext& resCtx,
            const QnConnectionInfo& connectionInfo );
        virtual ~RemoteEC2Connection();

        virtual QnConnectionInfo connectionInfo() const override;
        virtual void startReceivingNotifications( bool fullSyncRequired ) override;

    private:
        FixedUrlClientQueryProcessorPtr m_queryProcessor;
        const QnConnectionInfo m_connectionInfo;
        QnTransactionMessageBus* m_transactionMsg;
        QUrl m_peerUrl;
    };
}

#endif  //REMOTE_EC_CONNECTION_H
