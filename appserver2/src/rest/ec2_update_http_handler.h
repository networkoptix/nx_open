/**********************************************************
* 29 jan 2014
* akolesnikov
***********************************************************/

#ifndef EC2_UPDATE_HTTP_HANDLER_H
#define EC2_UPDATE_HTTP_HANDLER_H

#include <QtCore/QByteArray>
#include <QtCore/QMutex>
#include <QtCore/QMutexLocker>
#include <QtCore/QWaitCondition>

#include <common/common_module.h>

#include <rest/server/request_handler.h>
#include <utils/network/http/httptypes.h>
#include <transaction/transaction.h>

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
        UpdateHttpHandler( Ec2DirectConnectionPtr  connection )
        :
            m_connection( connection )
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
            QnInputBinaryStream<QByteArray> stream( body );
            if (!QnBinary::deserialize(&stream, &tran))
                return nx_http::StatusCode::badRequest;
            
            // replace client GUID to own GUID (take transaction ownership).
            tran.id.peerGUID = qnCommon->moduleGUID();

            ErrorCode errorCode = ErrorCode::ok;
            bool finished = false;

            auto queryDoneHandler = [&errorCode, &finished, this]( ErrorCode _errorCode )
            {
                errorCode = _errorCode;
                QMutexLocker lk( &m_mutex );
                finished = true;
                m_cond.wakeAll();
            };
            m_connection->queryProcessor()->processUpdateAsync( tran, queryDoneHandler );

            QMutexLocker lk( &m_mutex );
            while( !finished )
                m_cond.wait( lk.mutex() );

             // update local data
            if (errorCode == ErrorCode::ok)
                m_connection->triggerNotification(tran);

            return (errorCode == ErrorCode::ok)
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
        Ec2DirectConnectionPtr m_connection;
        QWaitCondition m_cond;
        QMutex m_mutex;
    };
}

#endif  //EC2_UPDATE_HTTP_HANDLER_H
