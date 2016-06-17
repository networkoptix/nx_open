/**********************************************************
* 29 jan 2014
* akolesnikov
***********************************************************/

#ifndef EC2_UPDATE_HTTP_HANDLER_H
#define EC2_UPDATE_HTTP_HANDLER_H

#include <QtCore/QByteArray>
#include <nx/utils/thread/mutex.h>
#include <nx/utils/thread/mutex.h>
#include <nx/utils/thread/wait_condition.h>

#include <common/common_module.h>

#include <rest/server/request_handler.h>
#include <nx/network/http/httptypes.h>
#include <nx/fusion/model_functions.h>
#include <transaction/transaction.h>

#include "server_query_processor.h"
#include "rest/server/json_rest_result.h"
#include "rest/server/rest_connection_processor.h"
#include "audit/audit_manager.h"
#include "api/model/audit/auth_session.h"
#include "audit/audit_manager.h"

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
            QByteArray&, /*contentType*/
            const QnRestConnectionProcessor*) override
        {
            return nx_http::StatusCode::badRequest;
        }

        //!Implementation of QnRestRequestHandler::executePost
        virtual int executePost(
            const QString& path,
            const QnRequestParamList& /*params*/,
            const QByteArray& body,
            const QByteArray& srcBodyContentType,
            QByteArray& resultBody,
            QByteArray& contentType,
            const QnRestConnectionProcessor* owner) override
        {
            QnTransaction<RequestDataType> tran;
            bool success = false;
            QByteArray srcFormat = srcBodyContentType.split(';')[0];
            Qn::SerializationFormat format = Qn::serializationFormatFromHttpContentType(srcFormat);
            switch( format )
            {
                //case Qn::BnsFormat:
                //    tran = QnBinary::deserialized<QnTransaction<RequestDataType>>(body);
                //    break;
                case Qn::JsonFormat:
                {
                    contentType = "application/json";
                    tran.params = QJson::deserialized<RequestDataType>(body, RequestDataType(), &success);
                    QStringList tmp = path.split('/');
                    while (!tmp.isEmpty() && tmp.last().isEmpty())
                        tmp.pop_back();
                    if (!tmp.isEmpty())
                        tran.command = ApiCommand::fromString(tmp.last());
                    break;
                }
                case Qn::UbjsonFormat:
                    tran = QnUbjson::deserialized<QnTransaction<RequestDataType>>(body, QnTransaction<RequestDataType>(), &success);
                    break;
                case Qn::UnsupportedFormat:
                    return nx_http::StatusCode::internalServerError;
                //case Qn::CsvFormat:
                //    tran = QnCsv::deserialized<QnTransaction<RequestDataType>>(body);
                //    break;
                //case Qn::XmlFormat:
                //    tran = QnXml::deserialized<QnTransaction<RequestDataType>>(body);
                //    break;
                default:
                    return nx_http::StatusCode::notAcceptable;
            }
            if (!success) {
                if (format == Qn::JsonFormat)
                {
                    QnJsonRestResult jsonResult;
                    jsonResult.setError(QnJsonRestResult::InvalidParameter, "Can't deserialize input Json data to destination object.");
                    resultBody = QJson::serialized(jsonResult);
                    return nx_http::StatusCode::ok;
                }
                else {
                    return nx_http::StatusCode::internalServerError;
                }
            }

            // replace client GUID to own GUID (take transaction ownership).
            tran.peerID = qnCommon->moduleGUID();
            tran.deliveryInfo.originatorType = QnTranDeliveryInformation::client;

            ErrorCode errorCode = ErrorCode::ok;
            bool finished = false;

            auto queryDoneHandler = [&errorCode, &finished, this]( ErrorCode _errorCode )
            {
                errorCode = _errorCode;
                QnMutexLocker lk( &m_mutex );
                finished = true;
                m_cond.wakeAll();
            };
            m_connection->queryProcessor()->getAccess(Qn::UserAccessData(owner->authUserId())).processUpdateAsync( tran, queryDoneHandler );

            {
                QnMutexLocker lk( &m_mutex );
                while( !finished )
                    m_cond.wait( lk.mutex() );
            }

            if( m_customAction )
                m_customAction( tran );

             // update local data
            if (errorCode == ErrorCode::ok) {
                m_connection->auditManager()->addAuditRecord(tran.command, tran.params, owner->authSession()); // add audit record before notification to ensure removed resource still alive
                m_connection->notificationManager()->triggerNotification(tran);
            }

            return (errorCode == ErrorCode::ok)
                ? nx_http::StatusCode::ok
                : nx_http::StatusCode::internalServerError;
        }

    private:
        Ec2DirectConnectionPtr m_connection;
        QnWaitCondition m_cond;
        QnMutex m_mutex;
        CustomActionFuncType m_customAction;
    };
}

#endif  //EC2_UPDATE_HTTP_HANDLER_H
