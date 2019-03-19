#pragma once

#include <functional>
#include <type_traits> //< for std::enable_if, std::is_same

#include <QtCore/QByteArray>

#include <nx/utils/log/assert.h>
#include <nx/utils/thread/mutex.h>
#include <nx/utils/thread/wait_condition.h>
#include <nx/network/http/http_types.h>
#include <nx/fusion/model_functions.h>
#include <api/model/password_data.h>

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
    class ProcessedRequestData = RequestData,
    class Connection = BaseEc2Connection<ServerQueryProcessorAccess> //< Overridable for tests.
>
class UpdateHttpHandler: public QnRestRequestHandler
{
public:
    typedef std::function<void(const ProcessedRequestData)> CustomActionFunc;
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
            case ErrorCode::badRequest:
                resultBody.clear();
                return nx::network::http::StatusCode::badRequest;
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
                    return nx::network::http::StatusCode::badRequest;
                break;
            }

            default:
                QnJsonRestResult::writeError(outResultBody, QnJsonRestResult::InvalidParameter,
                    lit("Unsupported Content Type: \"%1\".").arg(QString(requestContentType)));
                return nx::network::http::StatusCode::unsupportedMediaType;
        }

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
        nx::vms::api::IdData apiIdData(requestData->getIdForMerging());
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
     * Sfinae: Called when RequestData does not provide id for merging or is IdData (because
     * merging for API parameters of exact type IdData has no sence) - do not perform the merge.
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
     * Sfinae: Called when RequestData provides id for merging and is not IdData (because
     * merging for API parameters of exact type IdData has no sence) - attempt the merge.
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
        typename std::enable_if<!std::is_same<nx::vms::api::IdData, T>::value>::type* = nullptr)
    {
        const QnUuid id = requestData->getIdForMerging();
        if (id.isNull()) //< Id is omitted from request json - do not perform merge.
        {
            fillRequestDataIdIfPossible(requestData);
            return makeSuccess(requestData, outResultBody, outSuccess);
        }

        *outSuccess = false;

        ProcessedRequestData existingData;
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
            case ErrorCode::badRequest:
            {
                QnJsonRestResult::writeError(outResultBody, QnJsonRestResult::BadRequest,
                    "Bad request.");
                return nx::network::http::StatusCode::badRequest;
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
        QString errorMessage;
        if (!mergeJsonValues(&jsonValue, incompleteJsonValue, &errorMessage))
        {
            QnJsonRestResult::writeError(outResultBody, QnJsonRestResult::CantProcessRequest,
                errorMessage);
            return nx::network::http::StatusCode::badRequest;
        }

        if (!QJson::deserialize<RequestData>(jsonValue, requestData))
        {
            QnJsonRestResult::writeError(outResultBody, QnJsonRestResult::CantProcessRequest,
                "Unable to deserialize merged Json data to destination object.");
            return nx::network::http::StatusCode::badRequest;
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
     * Sfinae: Used only when RequestData provides id for merging but is not IdData (because
     * merging for API parameters of exact type IdData has no sence).
     */
    template<typename T>
    ErrorCode processQueryAsync(
        const QnUuid& uuid,
        T* outData,
        bool* outFound,
        const QnRestConnectionProcessor* owner,
        decltype(&T::getIdForMerging) /*enable_if_member_exists*/ = nullptr,
        typename std::enable_if<!std::is_same<nx::vms::api::IdData, T>::value>::type* = nullptr)
    {
        typedef std::vector<T> RequestDataList;

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

        // This call is used to merge data with existing to partial object update.
        // Access right to the object will be checked on a object update phase.
        m_connection->queryProcessor()->getAccess(Qn::kSystemAccess)
            .template processQueryAsync<QnUuid, RequestDataList, decltype(queryDoneHandler)>(
                /*unused*/ ApiCommand::NotDefined, uuid, queryDoneHandler);

        {
            QnMutexLocker lk(&m_mutex);
            while(!finished)
                m_cond.wait(lk.mutex());
        }

        return errorCode;
    }

    template<typename T>
    ErrorCode processUpdateAsync(
        ApiCommand::Value command,
        const T& requestData,
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

    bool mergeJsonValues(
        QJsonValue* existingValue,
        const QJsonValue& incompleteValue,
        QString* outErrorMessage,
        const QString& fieldName = QString())
    {
        auto toString = [](QJsonValue::Type jsonType)
        {
            switch(jsonType)
            {
                case QJsonValue::Null:
                    return "Null";
                case QJsonValue::Bool:
                    return "Bool";
                case QJsonValue::Double:
                    return "Double";
                case QJsonValue::String:
                    return "String";
                case QJsonValue::Array:
                    return "Array";
                case QJsonValue::Object:
                    return "Object";
                default:
                    return "Undefined";
            }
        };

        if (incompleteValue.type() == QJsonValue::Undefined //< Missing field type, as per Qt doc.
            || incompleteValue.type() == QJsonValue::Null) //< Missing field type, actual.
        {
            NX_VERBOSE(this, "        Incomplete value field is missing - ignored");
            return true; //< leave recursion
        }

        NX_VERBOSE(this, "BEGIN merge:");
        NX_VERBOSE(this, lm("    Existing:   %1").arg(QJson::serialize(*existingValue)));
        NX_VERBOSE(this, lm("    Incomplete: %1").arg(QJson::serialize(incompleteValue)));

        if (incompleteValue.type() != existingValue->type())
        {
            const QString fieldReference =
                fieldName.isEmpty() ? "" : lm(" field \"%1\"").arg(fieldName);
            *outErrorMessage = lm("Request%1 has invalid type. Expected type \"%2\", actual type \"%3\"")
                .args(fieldReference, toString(existingValue->type()), toString(incompleteValue.type()));
            NX_WARNING(this, *outErrorMessage);
            return false;
        }

        switch (existingValue->type())
        {
            case QJsonValue::Bool:
            case QJsonValue::Double:
            case QJsonValue::String:
            case QJsonValue::Array: //< Arrays treated as scalars - no items merging is performed.
                NX_VERBOSE(this, "Merging: Scalar or array - replacing");
                *existingValue = incompleteValue;
                break;

            case QJsonValue::Object:
            {
                NX_VERBOSE(this, "Merging: Object - process recursively:");
                QJsonObject object = existingValue->toObject();
                for (auto it = object.begin(); it != object.end(); ++it)
                {
                    NX_VERBOSE(this, lm("    Field \"%1\":").arg(it.key()));
                    QJsonValue field = it.value();
                    if (!mergeJsonValues(&field, incompleteValue.toObject()[it.key()], outErrorMessage, it.key())) //< recursion
                        return false;
                    it.value() = field; //< Write back possibly changed field.
                    NX_VERBOSE(this, lm("    Assigned %1").arg(QJson::serialize(it.value())));
                }
                *existingValue = object; //< Write back possibly changed object.
                break;
            }

            default:
                NX_VERBOSE(this, "Merging: Unknown type - ignored");
        }

        NX_VERBOSE(this, lm("END merge: new value: %1").arg(QJson::serialize(*existingValue)));
        return true;
    }

private:
    ConnectionPtr m_connection;
    QnWaitCondition m_cond;
    QnMutex m_mutex;
    CustomActionFunc m_customAction;
};

} // namespace ec2
