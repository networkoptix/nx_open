// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <cstdint>

#include <QtCore/QString>

#include <nx/network/http/http_types.h>
#include <nx/utils/impl_ptr.h>

namespace nx::network::http {

class NX_NETWORK_API HarEntry
{
public:
    HarEntry() = default;
    virtual ~HarEntry() = default;

    virtual void setRequest(nx::network::http::Method method,
        QString url,
        nx::network::http::MimeProtoVersion httpVersion,
        nx::network::http::HttpHeaders headers = {}) = 0;

    virtual void setServerAddress(const std::string& address, quint16 port) = 0;

    virtual void setResponseHeaders(
        const StatusLine& statusLine,
        const nx::network::http::HttpHeaders& headers) = 0;

    virtual void setError(const std::string& text) = 0;

    /**
     * @param body Request body with Content-Encoding (if any) already decoded.
     */
    virtual void setRequestBody(const nx::Buffer& body) = 0;

    /**
     * Number of request body bytes actually transferred, i.e. before Content-Encoding is
     * decoded. If not set, the size of the body passed to setRequestBody() is reported.
     */
    virtual void setRequestBodySize(std::uint64_t transferredSize) = 0;

    /**
     * @param body Response body with Content-Encoding (if any) already decoded.
     */
    virtual void setResponseBody(const nx::Buffer& body) = 0;

    /**
     * Number of response body bytes actually transferred, i.e. before Content-Encoding is
     * decoded. If not set, the size of the body passed to setResponseBody() is reported.
     */
    virtual void setResponseBodySize(std::uint64_t transferredSize) = 0;

    /**
     * Size of the whole decoded response body. Needed if the body passed to setResponseBody() is
     * incomplete, e.g. because the client has already fetched parts of it. If not set, the size of
     * that body is reported.
     */
    virtual void setResponseContentSize(std::uint64_t decodedSize) = 0;

    virtual void markConnectStart() = 0;
    virtual void markConnectDone() = 0;
    virtual void markRequestSent() = 0;
};

class NX_NETWORK_API Har
{
    friend struct EntryImpl;
    Har();

public:
    static Har* instance();

    void setFileName(const QString& fileName);

    virtual ~Har();

    std::unique_ptr<HarEntry> log();

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::network::http
