#pragma once

#include <functional>
#include <type_traits> //< for std::enable_if, std::is_same

#include <QtCore/QByteArray>

#include <nx/utils/log/assert.h>
#include <nx/utils/thread/mutex.h>
#include <nx/utils/thread/wait_condition.h>
#include <nx/network/http/httptypes.h>
#include <nx/fusion/model_functions.h>

#include <common/common_module.h>
#include <rest/server/request_handler.h>
#include <transaction/transaction.h>
#include <rest/server/json_rest_result.h>
#include <rest/server/rest_connection_processor.h>
#include <audit/audit_manager.h>

#include "ec2_connection.h"
#include "server_query_processor.h"

namespace ec2 {

/**
 * RestAPI request handler for update requests.
 */
template<
    class RequestData,
    class Connection = BaseEc2Connection<ServerQueryProcessorAccess> //< Overridable for tests.
>
class UpdateHttpHandler: public QnRestRequestHandler
{
public:
    typedef std::function<void(const QnTransaction<RequestData>&)> CustomActionFuncType;
    typedef std::shared_ptr<Connection> ConnectionPtr;

    explicit UpdateHttpHandler(
        const ConnectionPtr& connection,
        CustomActionFuncType customAction = CustomActionFuncType())
        :
        m_connection(connection),
        m_customAction(customAction)
    {
    }

    virtual int executeGet(
        const QString& /*path*/,
        const QnRequestParamList& /*params*/,
        QByteArray& /*result*/,
        QByteArray& /*contentType*/,
        const QnRestConnectionProcessor* /*owner*/) override
    {
        return nx_http::StatusCode::badRequest;
    }

    virtual int executePost(
        const QString& path,
        const QnRequestParamList& /*params*/,
        const QByteArray& body,
        const QByteArray& srcBodyContentType,
        QByteArray& resultBody,
        QByteArray& contentType,
        const QnRestConnectionProcessor* owner) override
    {
        const QByteArray srcFormat = srcBodyContentType.split(';')[0];

        const Qn::SerializationFormat format =
            Qn::serializationFormatFromHttpContentType(srcFormat);

        QnTransaction<RequestData> tran;
        bool success = false;
        auto httpStatusCode = buildTransaction(
            &tran, format, path, body, &resultBody, &contentType, &success, owner);
        if (!success)
            return httpStatusCode;

        // Replace client GUID to own GUID (take transaction ownership).
        tran.peerID = qnCommon->moduleGUID();

        switch (processUpdateAsync(tran, owner))
        {
            case ErrorCode::ok: return nx_http::StatusCode::ok;
            case ErrorCode::forbidden: return nx_http::StatusCode::forbidden;
            default: return nx_http::StatusCode::internalServerError;
        }
    }

private:
    nx_http::StatusCode::Value buildTransaction(
        QnTransaction<RequestData>* tran,
        Qn::SerializationFormat format,
        const QString& path,
        const QByteArray& body,
        QByteArray* outResultBody,
        QByteArray* outContentType,
        bool* outSuccess,
        const QnRestConnectionProcessor* owner)
    {
        *outSuccess = false;
        switch (format)
        {
            case Qn::JsonFormat:
            {
                *outContentType = "application/json";

                auto httpStatusCode = buildTransactionFromJson(
                    tran, path, body, outResultBody, outSuccess, owner);
                if (!*outSuccess)
                    return httpStatusCode;
                break;
            }
            case Qn::UbjsonFormat:
            {
                *tran = QnUbjson::deserialized<QnTransaction<RequestData>>(
                    body, QnTransaction<RequestData>(), outSuccess);
                if (!*outSuccess) //< Ubjson deserialization error.
                    return nx_http::StatusCode::internalServerError;
                break;
            }
            case Qn::UnsupportedFormat:
                return nx_http::StatusCode::internalServerError;

#if 0 // Deserialization for these formats is not implemented yet.
            case Qn::CsvFormat:
                *tran = QnCsv::deserialized<QnTransaction<RequestData>>(body);
                break;
            case Qn::XmlFormat:
                *tran = QnXml::deserialized<QnTransaction<RequestData>>(body);
                break;
#endif // 0

            default:
                return nx_http::StatusCode::notAcceptable;
        }
        *outSuccess = true;
        return nx_http::StatusCode::ok;
    }

    nx_http::StatusCode::Value buildTransactionFromJson(
        QnTransaction<RequestData>* tran,
        const QString& path,
        const QByteArray& body,
        QByteArray* outResultBody,
        bool* outSuccess,
        const QnRestConnectionProcessor* owner)
    {
        *outSuccess = false;

        boost::optional<QJsonValue> incompleteJsonValue;
        if (!QJson::deserializeAllowingOmittedValues(body, &tran->params, &incompleteJsonValue))
        {
            QnJsonRestResult::writeError(outResultBody, QnJsonRestResult::InvalidParameter,
                "Can't deserialize input Json data to destination object.");
            return nx_http::StatusCode::ok;
        }

        if (incompleteJsonValue)
        {
            // If transaction contains incomplete data, but merge is not possible (e.g. Id field is
            // omitted, object to merge with is not found in DB by Id, object type does not include
            // Id field), the behavior is the same as before introducing "merge" feature - attempt
            // such incomplete transaction with default values for omitted json fields.
            auto httpStatusCode = buildTransactionMergingIfNeeded(
                incompleteJsonValue.get(), tran, outResultBody, outSuccess, owner, tran->params);
            if (!*outSuccess)
                return httpStatusCode;
        }

        QString commandStr = assignCommandFromLastPathItem(&tran->command, path);
        if (tran->command == ApiCommand::NotDefined)
        {
            QnJsonRestResult::writeError(outResultBody, QnJsonRestResult::InvalidParameter,
                lit("Unknown API command %1").arg(commandStr));
            return nx_http::StatusCode::ok;
        }

        // TODO: Temporary fix. Revert this check after merge with x_VMS-3408_cloud_sync.
        if (tran->command == ApiCommand::restoreDatabase)
            tran->isLocal = true;

        *outSuccess = true;
        return nx_http::StatusCode::ok;
    }

    // Called when RequestData is not inherited from ApiIdData - do not perform merge.
    template<typename T = RequestData>
    nx_http::StatusCode::Value buildTransactionMergingIfNeeded(
        const QJsonValue& incompleteJsonValue,
        QnTransaction<RequestData>* tran,
        QByteArray* outResultBody,
        bool* outSuccess,
        const QnRestConnectionProcessor* owner,
        const T& requestData,
        typename std::enable_if<!std::is_base_of<ApiIdData, T>::value
            || std::is_same<ApiIdData, T>::value>::type* = nullptr)
    {
        QN_UNUSED(incompleteJsonValue, tran, outResultBody, owner, requestData);
        *outSuccess = true;
        return nx_http::StatusCode::ok;
    }

    // Called when RequestData is inherited from ApiIdData - attempt the merge.
    template<typename T = RequestData>
    nx_http::StatusCode::Value buildTransactionMergingIfNeeded(
        const QJsonValue& incompleteJsonValue,
        QnTransaction<RequestData>* tran,
        QByteArray* outResultBody,
        bool* outSuccess,
        const QnRestConnectionProcessor* owner,
        const T& requestData,
        typename std::enable_if<std::is_base_of<ApiIdData, T>::value
            && !std::is_same<ApiIdData, T>::value>::type* = nullptr)
    {
        if (requestData.id.isNull()) //< Id is omitted from request json - do not perform merge.
        {
            *outSuccess = true;
            return nx_http::StatusCode::ok;
        }

        *outSuccess = false;

        RequestData existingData;
        bool found = false;
        switch (processQueryAsync(requestData.id, &existingData, &found, owner))
        {
            case ErrorCode::ok:
                if (!found)
                {
                    *outSuccess = true;
                    return nx_http::StatusCode::ok;
                }
                break;

            case ErrorCode::forbidden:
            {
                QnJsonRestResult::writeError(outResultBody, QnJsonRestResult::Forbidden,
                    "Unable to retrieve existing object to merge with.");
                return nx_http::StatusCode::forbidden;
            }

            default:
            {
                QnJsonRestResult::writeError(outResultBody, QnJsonRestResult::CantProcessRequest,
                    "Unable to retrieve existing object to merge with.");
                return nx_http::StatusCode::internalServerError;
            }
        }

        QJsonValue jsonValue;
        QJson::serialize(existingData, &jsonValue);
        mergeJsonValues(&jsonValue, incompleteJsonValue);

        if (!QJson::deserialize<RequestData>(jsonValue, &tran->params))
        {
            QnJsonRestResult::writeError(outResultBody, QnJsonRestResult::CantProcessRequest,
                "Unable to deserialize merged Json data to destination object.");
            return nx_http::StatusCode::internalServerError;
        }

        *outSuccess = true;
        return nx_http::StatusCode::ok;
    }

    QString assignCommandFromLastPathItem(ApiCommand::Value* outCommand, const QString& path)
    {
        QStringList tmp = path.split('/');
        while (!tmp.isEmpty() && tmp.last().isEmpty())
            tmp.pop_back();
        QString commandStr;
        if (!tmp.isEmpty())
        {
            commandStr = tmp.last();
            *outCommand = ApiCommand::fromString(commandStr);
        }
        return commandStr;
    }

    template<typename T = RequestData>
    ErrorCode processQueryAsync(const QnUuid& uuid, T* outData, bool* outFound,
        const QnRestConnectionProcessor* owner,
        typename std::enable_if<std::is_base_of<ApiIdData, T>::value
            && !std::is_same<ApiIdData, T>::value>::type* = nullptr)
    {
        typedef std::vector<RequestData> RequestDataList;

        ErrorCode errorCode = ErrorCode::ok;
        bool finished = false;

        auto queryDoneHandler =
            [&outFound, &outData, &errorCode, &finished, this](
                ErrorCode _errorCode, RequestDataList list)
            {
                errorCode = _errorCode;
                if (errorCode == ErrorCode::ok)
                {
                    *outFound = list.size() > 0;
                    if (*outFound)
                    {
                        NX_ASSERT(list.size() == 1);
                        *outData = list[0];
                    }
                }
                QnMutexLocker lk(&m_mutex);
                finished = true;
                m_cond.wakeAll();
            };

        m_connection->queryProcessor()->getAccess(owner->accessRights())
            .template processQueryAsync<QnUuid, RequestDataList, decltype(queryDoneHandler)>(
                /*unused*/ ApiCommand::NotDefined, uuid, queryDoneHandler);

        {
            QnMutexLocker lk(&m_mutex);
            while(!finished)
                m_cond.wait(lk.mutex());
        }

        return errorCode;
    }

    ErrorCode processUpdateAsync(
        QnTransaction<RequestData>& tran, const QnRestConnectionProcessor* owner)
    {
        ErrorCode errorCode = ErrorCode::ok;
        bool finished = false;

        auto queryDoneHandler =
            [&errorCode, &finished, this](ErrorCode _errorCode)
            {
                errorCode = _errorCode;
                QnMutexLocker lk(&m_mutex);
                finished = true;
                m_cond.wakeAll();
            };

        auto processor = m_connection->queryProcessor()->getAccess(owner->accessRights());
        processor.setAuditData(m_connection->auditManager(), owner->authSession()); //< audit trail
        processor.processUpdateAsync(tran, queryDoneHandler);

        {
            QnMutexLocker lk(&m_mutex);
            while(!finished)
                m_cond.wait(lk.mutex());
        }

        if (m_customAction)
            m_customAction(tran);

        return errorCode;
    }

    void mergeJsonValues(QJsonValue* existingValue, const QJsonValue& incompleteValue)
    {
        if (incompleteValue.type() == QJsonValue::Undefined //< Missing field type, as per Qt doc.
            || incompleteValue.type() == QJsonValue::Null) //< Missing field type, actual.
        {
            NX_LOG("        Incomplete value field is missing - ignored", cl_logDEBUG2);
            return; //< leave recursion
        }

        NX_LOG("BEGIN merge:", cl_logDEBUG2);
        NX_LOG(lm("    Existing:   %1").arg(QJson::serialize(*existingValue)), cl_logDEBUG2);
        NX_LOG(lm("    Incomplete: %1").arg(QJson::serialize(incompleteValue)), cl_logDEBUG2);

        if (incompleteValue.type() != existingValue->type())
        {
            NX_LOG(lm("INTERNAL ERROR: Incomplete value type %1, existing value type %2")
                .arg(int(incompleteValue.type())).arg(int(existingValue->type())), cl_logDEBUG2);
            NX_ASSERT(false);
            return;
        }

        switch (existingValue->type())
        {
            case QJsonValue::Bool:
            case QJsonValue::Double:
            case QJsonValue::String:
            case QJsonValue::Array: //< Arrays treated as scalars - no items merging is performed.
                NX_LOG("Mering: Scalar or array - replacing", cl_logDEBUG2);
                *existingValue = incompleteValue;
                break;

            case QJsonValue::Object:
            {
                NX_LOG("Merging: Object - process recursively:", cl_logDEBUG2);
                QJsonObject object = existingValue->toObject();
                for (auto it = object.begin(); it != object.end(); ++it)
                {
                    NX_LOG(lm("    Field \"%1\":").arg(it.key()), cl_logDEBUG2);
                    QJsonValue field = it.value();
                    mergeJsonValues(&field, incompleteValue.toObject()[it.key()]); //< recursion
                    it.value() = field; //< Write back possibly changed field.
                    NX_LOG(lm("    Assigned %1").arg(QJson::serialize(it.value())), cl_logDEBUG2);
                }
                *existingValue = object; //< Write back possibly changed object.
                break;
            }

            default:
                NX_LOG("Merging: Unknown type - ignored", cl_logDEBUG2);
        }

        NX_LOG(lm("END merge: new value: %1").arg(QJson::serialize(*existingValue)),
            cl_logDEBUG2);
    }

private:
    ConnectionPtr m_connection;
    QnWaitCondition m_cond;
    QnMutex m_mutex;
    CustomActionFuncType m_customAction;
};

} // namespace ec2
