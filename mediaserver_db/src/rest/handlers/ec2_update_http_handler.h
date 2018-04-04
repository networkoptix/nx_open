#pragma once

#include <functional>
#include <type_traits> //< for std::enable_if, std::is_same

#include <QtCore/QByteArray>

#include <nx/utils/log/assert.h>
#include <nx/utils/thread/mutex.h>
#include <nx/utils/thread/wait_condition.h>
#include <nx/network/http/http_types.h>
#include <nx/fusion/model_functions.h>

#include <rest/server/request_handler.h>
#include <transaction/transaction.h>
#include <rest/server/json_rest_result.h>
#include <rest/server/rest_connection_processor.h>
#include <audit/audit_manager.h>

#include "ec2_connection.h"
#include "server_query_processor.h"

namespace ec2 {

namespace update_http_handler_detail {

// TODO: Move to ApiIdData. Then consider moving this and fillId() methods out of Api*Data.

template<typename RequestData>
inline void fixRequestDataIfNeeded(RequestData* const /*requestData*/)
{
}

template<>
inline void fixRequestDataIfNeeded<ApiUserData>(ApiUserData* const userData)
{
    if (userData->isCloud)
    {
        if (userData->name.isEmpty())
            userData->name = userData->email;

        if (userData->digest.isEmpty())
            userData->digest = ApiUserData::kCloudPasswordStub;
    }
}

} // namespace update_http_handler_detail

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
    typedef std::function<void(const RequestData)> CustomActionFunc;
    typedef std::shared_ptr<Connection> ConnectionPtr;

    explicit UpdateHttpHandler(
        const ConnectionPtr& connection,
        CustomActionFunc customAction = CustomActionFunc())
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
        return nx::network::http::StatusCode::badRequest;
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
        const ApiCommand::Value command = extractCommandFromPath(path);
        if (command == ApiCommand::NotDefined)
            return nx::network::http::StatusCode::notFound;

        const QByteArray requestContentType = srcBodyContentType.split(';')[0];

        RequestData requestData;
        bool success = false;
        auto httpStatusCode = buildRequestData(
            &requestData, requestContentType, body, &resultBody, &contentType, &success, owner);
        if (!success)
            return httpStatusCode;

        switch (processUpdateAsync(command, requestData, owner))
        {
            case ErrorCode::ok:
                return nx::network::http::StatusCode::ok;
            case ErrorCode::forbidden:
                resultBody.clear();
                return nx::network::http::StatusCode::forbidden;
            default:
                resultBody.clear();
                return nx::network::http::StatusCode::internalServerError;
        }
    }

private:
    nx::network::http::StatusCode::Value buildRequestData(
        RequestData* requestData,
        const QByteArray& requestContentType,
        const QByteArray& body,
        QByteArray* outResultBody,
        QByteArray* outContentType,
        bool* outSuccess,
        const QnRestConnectionProcessor* owner)
    {
        *outSuccess = false;

        const Qn::SerializationFormat format =
            Qn::serializationFormatFromHttpContentType(requestContentType);

        switch (format)
        {
            case Qn::JsonFormat:
            {
                *outContentType = "application/json";
                auto httpStatusCode = buildRequestDataFromJson(
                    requestData, body, outResultBody, outSuccess, owner);
                if (!*outSuccess)
                    return httpStatusCode;
                break;
            }

            case Qn::UbjsonFormat:
            {
                *requestData = QnUbjson::deserialized<RequestData>(
                    body, RequestData(), outSuccess);
                if (!*outSuccess) //< Ubjson deserialization error.
                    return nx::network::http::StatusCode::invalidParameter;
                break;
            }

            default:
                QnJsonRestResult::writeError(outResultBody, QnJsonRestResult::InvalidParameter,
                    lit("Unsupported Content Type: \"%1\".").arg(QString(requestContentType)));
                return nx::network::http::StatusCode::unsupportedMediaType;
        }

        update_http_handler_detail::fixRequestDataIfNeeded(requestData);

        return nx::network::http::StatusCode::ok;
    }

    nx::network::http::StatusCode::Value buildRequestDataFromJson(
        RequestData* requestData,
        const QByteArray& body,
        QByteArray* outResultBody,
        bool* outSuccess,
        const QnRestConnectionProcessor* owner)
    {
        *outSuccess = false;

        boost::optional<QJsonValue> incompleteJsonValue;
        if (!QJson::deserializeAllowingOmittedValues(body, requestData, &incompleteJsonValue))
        {
            QnJsonRestResult::writeError(outResultBody, QnJsonRestResult::InvalidParameter,
                "Can't deserialize input Json data to destination object.");
            return nx::network::http::StatusCode::ok;
        }

        if (incompleteJsonValue)
        {
            // If requestData contains incomplete data, but merge is not possible (e.g. Id field is
            // omitted, object to merge with is not found in DB by Id, object type does not include
            // Id field), the behavior is the same as before introducing "merge" feature - attempt
            // such incomplete request with default values for omitted json fields.
            return buildRequestDataMergingIfNeeded(
                requestData, incompleteJsonValue.get(), outResultBody, outSuccess, owner);
        }
        else
        {
            return makeSuccess(requestData, outResultBody, outSuccess);
        }
    }

    /**
     * Sfinae wrapper.
     */
    template<typename T = RequestData>
    void fillRequestDataIdIfPossible(RequestData* requestData)
    {
        fillRequestDataIdIfPossibleSfinae(requestData, /*enable_if_member_exists*/ nullptr);
    }

    /**
     * Sfinae: Called when RequestData does not provide fillId() - do nothing.
     */
    template<typename T = RequestData>
    void fillRequestDataIdIfPossibleSfinae(
        RequestData* /*requestData*/,
        ... /*enable_if_member_exists*/)
    {
    }

    /**
     * Sfinae: Called when RequestData provides fillId() - generate new id.
     */
    template<typename T = RequestData>
    void fillRequestDataIdIfPossibleSfinae(
        RequestData* requestData,
        decltype(&T::fillId) /*enable_if_member_exists*/)
    {
        requestData->fillId();
    }

    /**
     * Helper. Prepare API call result for successful execution.
     * If RequestData provides getIdForMerging(), fill outResultBody with {"id": "<objectId>"},
     * otherwise, set outResultBody to empty JSON object {}.
     * Sfinae wrapper.
     * @return HTTP OK (200), having set outSuccess to true.
     */
    template<typename T = RequestData>
    static nx::network::http::StatusCode::Value makeSuccess(
        const RequestData* requestData,
        QByteArray* outResultBody,
        bool* outSuccess)
    {
        return makeSuccessSfinae(
            requestData, outResultBody, outSuccess, /*enable_if_member_exists*/ nullptr);
    }

    /**
     * Sfinae: Called when RequestData provides getIdForMerging().
     */
    template<typename T = RequestData>
    static nx::network::http::StatusCode::Value makeSuccessSfinae(
        const RequestData* requestData,
        QByteArray* outResultBody,
        bool* outSuccess,
        decltype(&T::getIdForMerging) /*enable_if_member_exists*/)
    {
        ApiIdData apiIdData(requestData->getIdForMerging());
        QJson::serialize(apiIdData, outResultBody);
        *outSuccess = true;
        return nx::network::http::StatusCode::ok;
    }

    /**
     * Sfinae: Called when RequestData does not provide getIdForMerging().
     */
    template<typename T = RequestData>
    static nx::network::http::StatusCode::Value makeSuccessSfinae(
        const RequestData* /*requestData*/,
        QByteArray* outResultBody,
        bool* outSuccess,
        ... /*enable_if_member_exists*/)
    {
        *outResultBody = "{}";
        *outSuccess = true;
        return nx::network::http::StatusCode::ok;
    }

    /**
     * Sfinae wrapper.
     */
    template<typename T = RequestData>
    nx::network::http::StatusCode::Value buildRequestDataMergingIfNeeded(
        RequestData* requestData,
        const QJsonValue& incompleteJsonValue,
        QByteArray* outResultBody,
        bool* outSuccess,
        const QnRestConnectionProcessor* owner)
    {
        return buildRequestDataMergingIfNeededSfinae(
            requestData, incompleteJsonValue, outResultBody, outSuccess, owner,
            /*enable_if_member_exists*/ nullptr);
    }

    /**
     * Sfinae: Called when RequestData does not provide id for merging or is ApiIdData (because
     * merging for API parameters of exact type ApiIdData has no sence) - do not perform the merge.
     */
    template<typename T = RequestData>
    nx::network::http::StatusCode::Value buildRequestDataMergingIfNeededSfinae(
        RequestData* requestData,
        const QJsonValue& /*incompleteJsonValue*/,
        QByteArray* outResultBody,
        bool* outSuccess,
        const QnRestConnectionProcessor* /*owner*/,
        ... /*enable_if_member_exists*/)
    {
        return makeSuccess(requestData, outResultBody, outSuccess);
    }

    /**
     * Sfinae: Called when RequestData provides id for merging and is not ApiIdData (because
     * merging for API parameters of exact type ApiIdData has no sence) - attempt the merge.
     *
     * @param requestData In: potentially incomplete data. Out: merge result.
     */
    template<typename T = RequestData>
    nx::network::http::StatusCode::Value buildRequestDataMergingIfNeededSfinae(
        RequestData* requestData,
        const QJsonValue& incompleteJsonValue,
        QByteArray* outResultBody,
        bool* outSuccess,
        const QnRestConnectionProcessor* owner,
        decltype(&T::getIdForMerging) /*enable_if_member_exists*/,
        typename std::enable_if<!std::is_same<ApiIdData, T>::value>::type* = nullptr)
    {
        const QnUuid id = requestData->getIdForMerging();
        if (id.isNull()) //< Id is omitted from request json - do not perform merge.
        {
            fillRequestDataIdIfPossible(requestData);
            return makeSuccess(requestData, outResultBody, outSuccess);
        }

        *outSuccess = false;

        RequestData existingData;
        bool found = false;
        switch (processQueryAsync(id, &existingData, &found, owner))
        {
            case ErrorCode::ok:
                if (!found)
                    return makeSuccess(requestData, outResultBody, outSuccess);
                break;

            case ErrorCode::forbidden:
            {
                QnJsonRestResult::writeError(outResultBody, QnJsonRestResult::Forbidden,
                    "Unable to retrieve existing object to merge with.");
                return nx::network::http::StatusCode::forbidden;
            }

            default:
            {
                QnJsonRestResult::writeError(outResultBody, QnJsonRestResult::CantProcessRequest,
                    "Unable to retrieve existing object to merge with.");
                return nx::network::http::StatusCode::internalServerError;
            }
        }

        QJsonValue jsonValue;
        QJson::serialize(existingData, &jsonValue);
        mergeJsonValues(&jsonValue, incompleteJsonValue);

        if (!QJson::deserialize<RequestData>(jsonValue, requestData))
        {
            QnJsonRestResult::writeError(outResultBody, QnJsonRestResult::CantProcessRequest,
                "Unable to deserialize merged Json data to destination object.");
            return nx::network::http::StatusCode::internalServerError;
        }

        return makeSuccess(requestData, outResultBody, outSuccess);
    }

    static ApiCommand::Value extractCommandFromPath(const QString& path)
    {
        const auto& parts = path.split('/', QString::SkipEmptyParts);

        if (parts.isEmpty())
            return ApiCommand::NotDefined;

        QString commandStr = parts.last();
        return ApiCommand::fromString(commandStr);
    }

    /**
     * Sfinae: Used only when RequestData provides id for merging but is not ApiIdData (because
     * merging for API parameters of exact type ApiIdData has no sence).
     */
    template<typename T = RequestData>
    ErrorCode processQueryAsync(
        const QnUuid& uuid,
        RequestData* outData,
        bool* outFound,
        const QnRestConnectionProcessor* owner,
        decltype(&T::getIdForMerging) /*enable_if_member_exists*/ = nullptr,
        typename std::enable_if<!std::is_same<ApiIdData, T>::value>::type* = nullptr)
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
        ApiCommand::Value command,
        const RequestData& requestData,
        const QnRestConnectionProcessor* owner)
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
        processor.processUpdateAsync(command, requestData, queryDoneHandler);

        {
            QnMutexLocker lk(&m_mutex);
            while (!finished)
                m_cond.wait(lk.mutex());
        }

        if (m_customAction)
            m_customAction(requestData);

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
                NX_LOG("Merging: Scalar or array - replacing", cl_logDEBUG2);
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
    CustomActionFunc m_customAction;
};

} // namespace ec2
