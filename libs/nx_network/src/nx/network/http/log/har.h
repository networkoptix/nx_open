// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QString>

#include <nx/network/http/http_types.h>
#include <nx/utils/impl_ptr.h>

namespace nx::network::http {

class NX_NETWORK_API HarEntry
{
public:
    HarEntry() = default;
    virtual ~HarEntry() = default;

    virtual void setRequest(
        nx::network::http::Method method,
        const QString& url,
        const nx::network::http::HttpHeaders& headers = {}) = 0;

    virtual void setServerAddress(const std::string& address, quint16 port) = 0;

    virtual void setResponseHeaders(
        const StatusLine& statusLine,
        const nx::network::http::HttpHeaders& headers) = 0;

    virtual void setError(const std::string& text) = 0;

    virtual void setRequestBody(const nx::Buffer& body) = 0;
    virtual void setResponseBody(const nx::Buffer& body) = 0;

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
