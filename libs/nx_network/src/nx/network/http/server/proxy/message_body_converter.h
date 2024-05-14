// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>

#include <nx/network/socket_common.h>
#include <nx/utils/basic_factory.h>

#include "../../http_types.h"

namespace nx::network::http::server::proxy {

class NX_NETWORK_API AbstractMessageBodyConverter
{
public:
    virtual ~AbstractMessageBodyConverter() = default;

    virtual nx::Buffer convert(nx::Buffer originalBody) = 0;
};

class NX_NETWORK_API AbstractUrlRewriter
{
public:
    virtual ~AbstractUrlRewriter() = default;

    virtual nx::utils::Url originalResourceUrlToProxyUrl(
        const nx::utils::Url& originalResourceUrl,
        const nx::utils::Url& proxyHostUrl,
        const std::string& targetHost) const = 0;
};

//-------------------------------------------------------------------------------------------------

using MessageBodyConverterFactoryFunction =
    std::unique_ptr<AbstractMessageBodyConverter>(
        const nx::utils::Url& proxyHostUrl,
        const std::string& targetHost,
        const std::string& contentType);

class NX_NETWORK_API MessageBodyConverterFactory:
    public nx::utils::BasicFactory<MessageBodyConverterFactoryFunction>
{
    using base_type = nx::utils::BasicFactory<MessageBodyConverterFactoryFunction>;

public:
    MessageBodyConverterFactory();

    /**
     * NOTE: Default implementation does not do anything.
     */
    void setUrlConverter(std::unique_ptr<AbstractUrlRewriter> urlConverter);

    static MessageBodyConverterFactory& instance();

private:
    std::unique_ptr<AbstractUrlRewriter> m_urlConverter;

    std::unique_ptr<AbstractMessageBodyConverter> defaultFactoryFunction(
        const nx::utils::Url& proxyHostUrl,
        const std::string& targetHost,
        const std::string& contentType);
};

} // namespace nx::network::http::server::proxy
