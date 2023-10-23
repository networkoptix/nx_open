// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>
#include <QtCore/QUrlQuery>

#include <nx/fusion/model_functions.h>
#include <nx/network/ssl/helpers.h>
#include <nx/utils/impl_ptr.h>
#include <nx_ec/ec_api_common.h>
#include <rest/request_params.h>
#include <transaction/transaction.h>

namespace nx::vms::common { class AbstractCertificateVerifier; }

namespace nx::vms::client::core {

/**
 * Interface for transaction manager classes to send transactions in form of http requests.
 */
class QueryProcessor: public QObject
{
    Q_OBJECT

    using AbstractCertificateVerifier = nx::vms::common::AbstractCertificateVerifier;

public:
    QueryProcessor(
        const QnUuid& serverId,
        const QnUuid& runningInstanceId,
        AbstractCertificateVerifier* certificateVerifier,
        nx::network::SocketAddress address,
        nx::network::http::Credentials credentials,
        Qn::SerializationFormat serializationFormat);
    virtual ~QueryProcessor() override;

    void updateSessionId(const QnUuid& sessionId);

    QueryProcessor& getAccess(const Qn::UserSession& /*session*/) { return *this; }

private:
    void pleaseStopSync();

public:
    nx::network::SocketAddress address() const;
    void updateAddress(nx::network::SocketAddress value);

    nx::network::http::Credentials credentials() const;
    void updateCredentials(nx::network::http::Credentials value);

    nx::network::ssl::AdapterFunc adapterFunc(const QnUuid& serverId) const;

    using PostRequestHandler = nx::utils::MoveOnlyFunc<void(ec2::ErrorCode)>;

    /**
     * Send POST request.
     * @param handler Functor(ErrorCode)
     */
    template<class InputData>
    void processUpdateAsync(
        ec2::ApiCommand::Value cmdCode,
        InputData input,
        PostRequestHandler handler)
    {
        QByteArray serializedData;
        if (serializationFormat() == Qn::SerializationFormat::ubjson)
        {
            serializedData = QnUbjson::serialized(input);
        }
        else
        {
            NX_ASSERT(serializationFormat() == Qn::SerializationFormat::json);
            serializedData = QJson::serialized(input);
        }

        NX_DEBUG(this, "Update %1 as %2: %3",
            cmdCode, serializationFormat(), QJson::serialized(input));
        sendPostRequest(cmdCode, std::move(serializedData),
            [this, cmdCode, handler = std::move(handler)](auto result)
            {
                NX_DEBUG(this, "Update %1 result: %2", cmdCode, result);
                handler(result);
            });
    }

    /**
     * Send GET request.
     * @param handler Functor(ErrorCode, OutputData)
     */
    template<class InputData, class OutputData, class HandlerType>
    void processQueryAsync(
        ec2::ApiCommand::Value cmdCode,
        InputData input,
        HandlerType handler)
    {
        QUrlQuery query;
        ec2::toUrlParams(input, &query);

        auto internalHandler =
            [handler = std::move(handler)](
                ec2::ErrorCode errorCode,
                Qn::SerializationFormat format,
                const nx::Buffer& data) mutable
            {
                if (errorCode != ec2::ErrorCode::ok)
                {
                    handler(errorCode, OutputData());
                }
                else
                {
                    OutputData outputData;
                    bool success = false;
                    switch (format)
                    {
                        case Qn::SerializationFormat::json:
                            outputData = QJson::deserialized<OutputData>(
                                data, OutputData(), &success);
                            break;
                        case Qn::SerializationFormat::ubjson:
                            outputData = QnUbjson::deserialized<OutputData>(
                                data, OutputData(), &success);
                            break;
                        default:
                            NX_ASSERT(false);
                    }

                    if (!success)
                        handler(ec2::ErrorCode::badResponse, outputData);
                    else
                        handler(ec2::ErrorCode::ok, outputData);
                }
        };

        sendGetRequest(cmdCode, std::move(query), std::move(internalHandler));
    }

private:
    Qn::SerializationFormat serializationFormat() const;

    void sendGetRequest(
        ec2::ApiCommand::Value cmdCode,
        QUrlQuery query,
        nx::utils::MoveOnlyFunc<void(ec2::ErrorCode, Qn::SerializationFormat, const nx::Buffer&)> handler);

    void sendPostRequest(
        ec2::ApiCommand::Value cmdCode,
        QByteArray serializedData,
        PostRequestHandler handler);

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

using QueryProcessorPtr = std::shared_ptr<QueryProcessor>;

} // namespace nx::vms::client::core
