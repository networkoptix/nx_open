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
#include <utils/common/model_functions.h>
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
        typedef std::function<void(const QnTransaction<RequestDataType>&)> CustomActionFuncType;

        UpdateHttpHandler(
            const Ec2DirectConnectionPtr& connection,
            CustomActionFuncType customAction = CustomActionFuncType() )
        :
            m_connection( connection ),
            m_customAction( customAction )
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
            const QByteArray& srcBodyContentType,
            QByteArray& /*result*/,
            QByteArray& /*contentType*/ )
        {
            QnTransaction<RequestDataType> tran;

            Qn::SerializationFormat format = Qn::serializationFormatFromHttpContentType(srcBodyContentType);
            switch( format )
            {
                case Qn::BnsFormat:
                    tran = QnBinary::deserialized<QnTransaction<RequestDataType>>(body);
                    break;
                case Qn::JsonFormat:
                    tran = QJson::deserialized<QnTransaction<RequestDataType>>(body);
                    break;
                case Qn::UbjsonFormat:
                    tran = QnUbjson::deserialized<QnTransaction<RequestDataType>>(body);
                    break;
                //case Qn::CsvFormat:
                //    tran = QnCsv::deserialized<QnTransaction<RequestDataType>>(body);
                //    break;
                //case Qn::XmlFormat:
                //    tran = QnXml::deserialized<QnTransaction<RequestDataType>>(body);
                //    break;
                default:
                    assert(false);
            }

            // replace client GUID to own GUID (take transaction ownership).
            tran.peerID = qnCommon->moduleGUID();

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

            {
                QMutexLocker lk( &m_mutex );
                while( !finished )
                    m_cond.wait( lk.mutex() );
            }

            if( m_customAction )
                m_customAction( tran );

             // update local data
            if (errorCode == ErrorCode::ok)
                m_connection->notificationManager()->triggerNotification(tran);

            return (errorCode == ErrorCode::ok)
                ? nx_http::StatusCode::ok
                : nx_http::StatusCode::internalServerError;
        }

    private:
        Ec2DirectConnectionPtr m_connection;
        QWaitCondition m_cond;
        QMutex m_mutex;
        CustomActionFuncType m_customAction;
    };
}

#endif  //EC2_UPDATE_HTTP_HANDLER_H
