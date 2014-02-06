/**********************************************************
* 29 jan 2014
* akolesnikov
***********************************************************/

#ifndef EC2_UPDATE_HTTP_HANDLER_H
#define EC2_UPDATE_HTTP_HANDLER_H

#include <condition_variable>
#include <mutex>

#include <QtCore/QByteArray>

#include <rest/server/request_handler.h>
#include <utils/network/http/httptypes.h>

#include "server_query_processor.h"


namespace ec2
{
    //!Http request handler for update requests
    template<class RequestDataType>
    class UpdateHttpHandler
    :
        public QnRestRequestHandler
    {
    public:
        UpdateHttpHandler( ServerQueryProcessor* const queryProcessor )
        :
            m_queryProcessor( queryProcessor )
        {
        }

        //!Implementation of QnRestRequestHandler::executeGet
        virtual int executeGet(
            const QString& /*path*/,
            const QnRequestParamList& /*params*/,
            QByteArray& /*result*/,
            QByteArray& /*contentType*/ )
        {
            return nx_http::StatusCode::badRequest;
        }

        //!Implementation of QnRestRequestHandler::executePost
        virtual int executePost(
            const QString& /*path*/,
            const QnRequestParamList& /*params*/,
            const QByteArray& body,
            QByteArray& /*result*/,
            QByteArray& /*contentType*/ )
        {
            QnTransaction<RequestDataType> tran;
            //tran.command = ;
            tran.createNewID();
            InputBinaryStream<QByteArray> stream( body );
            QnBinary::deserialize( tran.params, &stream );

            ErrorCode errorCode = ErrorCode::ok;
            bool finished = false;

            auto queryDoneHandler = [&errorCode, &finished, this]( ErrorCode _errorCode )
            {
                errorCode = _errorCode;
                std::unique_lock<std::mutex> lk( m_mutex );
                finished = true;
                m_cond.notify_all();
            };
            m_queryProcessor->processUpdateAsync( tran, queryDoneHandler );

            std::unique_lock<std::mutex> lk( m_mutex );
            m_cond.wait( lk, [&finished]{ return finished; } );

            return errorCode == ErrorCode::ok
                ? nx_http::StatusCode::ok
                : nx_http::StatusCode::internalServerError;
        }

        //!Implementation of QnRestRequestHandler::description
        virtual QString description() const
        {
            //TODO/IMPL
            return QString();
        }

    private:
        ServerQueryProcessor* const m_queryProcessor;
        std::condition_variable m_cond;
        std::mutex m_mutex;
    };
}

#endif  //EC2_UPDATE_HTTP_HANDLER_H
