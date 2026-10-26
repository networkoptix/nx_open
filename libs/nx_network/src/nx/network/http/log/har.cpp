// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "har.h"

#include <chrono>
#include <optional>
#include <string>
#include <string_view>

#include <cpptrace/cpptrace.hpp>

#include <QtCore/QCoreApplication>
#include <QtCore/QDateTime>
#include <QtCore/QDir>
#include <QtCore/QJsonDocument>
#include <QtCore/QString>
#include <QtCore/QUrl>
#include <QtCore/QUrlQuery>

#include <nx/branding.h>
#include <nx/build_info.h>
#include <nx/kit/ini_config.h>
#include <nx/utils/log/format.h>
#include <nx/utils/log/log.h>
#include <nx/utils/std_string_utils.h>
#include <nx/utils/thread/mutex.h>

namespace {

using namespace std::chrono;

static const QByteArray kCloseEntries = "\n]}}\n";
static constexpr auto kFlushInterval = 10s;

QVariantList getFrameList(const cpptrace::raw_trace& rawTrace)
{
    const auto stackData = rawTrace.resolve();

    QVariantList stacktraceList;
    for (const auto& frame: stackData)
    {
        QVariantMap frameInfo;
        frameInfo["_address"] = QString("0x%1").arg((quintptr) (frame.raw_address), 0, 16);
        frameInfo["functionName"] = QString::fromStdString(frame.symbol);
        frameInfo["url"] = "file://" + QString::fromStdString(frame.filename);
        frameInfo["lineNumber"] = frame.line.has_value()
            ? static_cast<int>(frame.line.value())
            : -1;
        frameInfo["columnNumber"] = frame.column.has_value()
            ? static_cast<int>(frame.column.value())
            : -1;
        stacktraceList.append(frameInfo);
    }

    return stacktraceList;
}

static QString dateTimeAsString(QDateTime dateTime)
{
    dateTime.setTimeZone(QTimeZone::systemTimeZone());
    return dateTime.toString(Qt::ISODateWithMs);
}

static QString currentDateTimeAsString()
{
    return dateTimeAsString(QDateTime::currentDateTime());
}

} // namespace

namespace nx::network::http {

namespace {

std::string contentType(const HttpHeaders& headers)
{
    const auto it = headers.find(header::ContentType::NAME);
    return it == headers.end() ? std::string() : it->second;
}

/**
 * @return Lower-case media type without parameters, e.g. "application/soap+xml" for
 * "application/soap+xml; charset=utf-8".
 */
std::string mediaType(const std::string& contentType)
{
    return nx::utils::trim(header::ContentType(contentType).value);
}

bool isTextMediaType(std::string_view type)
{
    return type.starts_with("text/") || type == header::ContentType::kJson.value
        || type == header::ContentType::kXml.value || type == header::ContentType::kForm.value
        || type.ends_with("+json") //< E.g. "application/problem+json".
        || type.ends_with("+xml"); //< E.g. "application/soap+xml".
}

QString toHarHttpVersion(const MimeProtoVersion& version)
{
    return NX_FMT("%1/%2", version.protocol, version.version);
}

QVariantList toHarHeaders(const HttpHeaders& headers)
{
    QVariantList result;
    for (const auto& [name, value]: headers)
    {
        result.append(QVariantMap{
            {"name", QString::fromStdString(name)}, {"value", QString::fromStdString(value)}});
    }
    return result;
}

QVariantList toHarQueryString(const QString& url)
{
    QVariantList result;
    for (const auto& [name, value]: QUrlQuery(QUrl(url)).queryItems(QUrl::FullyDecoded))
        result.append(QVariantMap{{"name", name}, {"value", value}});
    return result;
}

} // namespace

// HAR (HTTP Archive) is a JSON-formatted archive file for logging client interaction.
// Spec: http://www.softwareishard.com/blog/har-12-spec

struct Har::Private
{
    std::atomic<bool> isEnabled = false;
    QFile harFile;
    nx::Mutex fileMutex;
    size_t entryCount = 0;
    QElapsedTimer flushTimer;
};

struct EntryImpl: public HarEntry
{
public:
    EntryImpl():
        m_startedDateTime(QDateTime::currentDateTime()),
        m_startTime(steady_clock::now()),
        m_connectStartTime(m_startTime)
    {
        m_stackTrace = cpptrace::generate_raw_trace();
    }

    ~EntryImpl() override
    {
        if (!hasAnyResponse())
            m_errorText = "Dropped request";

        write();
    }

    void setServerAddress(const std::string& address, quint16 port) override
    {
        m_serverAddress = address;
        m_port = port;
    }

    void setRequest(nx::network::http::Method method,
        QString url,
        nx::network::http::MimeProtoVersion httpVersion,
        nx::network::http::HttpHeaders headers) override
    {
        m_method = std::move(method);
        m_url = std::move(url);
        m_requestHttpVersion = std::move(httpVersion);
        m_requestHeaders = std::move(headers);
    }

    void setResponseHeaders(
        const StatusLine& statusLine,
        const HttpHeaders& headers) override
    {
        m_statusLine = statusLine;
        m_responseHeaders = headers;

        m_responseReceivedTime = steady_clock::now();
    }

    void setError(const std::string& text) override
    {
        m_errorText = text;
    }

    void setRequestBody(const nx::Buffer& body) override
    {
        m_requestBody = body;
    }

    void setRequestBodySize(std::uint64_t transferredSize) override
    {
        m_requestBodySize = transferredSize;
    }

    void setResponseBody(const nx::Buffer& body) override
    {
        m_responseBody = body;
        m_endTime = steady_clock::now();
    }

    void setResponseBodySize(std::uint64_t transferredSize) override
    {
        m_responseBodySize = transferredSize;
    }

    void setResponseContentSize(std::uint64_t decodedSize) override
    {
        m_responseContentSize = decodedSize;
    }

    void markConnectStart() override
    {
        m_connectStartTime = steady_clock::now();
    }

    void markConnectDone() override
    {
        m_connectDoneTime = steady_clock::now();
    }

    void markRequestSent() override
    {
        m_requestSentTime = steady_clock::now();
    }

    int getBlockedTime() const
    {
        if (m_connectStartTime < m_startTime)
            return -1;

        return duration_cast<milliseconds>(m_connectStartTime - m_startTime).count();
    }

    int getConnectTime() const
    {
        if (m_connectDoneTime < m_connectStartTime || m_connectStartTime < m_startTime)
            return -1;

        return duration_cast<milliseconds>(m_connectDoneTime - m_connectStartTime).count();
    }

    int getSendTime() const
    {
        // A reused connection is not connected, so sending starts right away.
        const auto sendStartTime =
            m_connectDoneTime < m_startTime ? m_startTime : m_connectDoneTime;

        if (m_requestSentTime < sendStartTime)
            return -1;

        return duration_cast<milliseconds>(m_requestSentTime - sendStartTime).count();
    }

    int getWaitTime() const
    {
        if (m_responseReceivedTime < m_requestSentTime || m_requestSentTime < m_startTime)
            return -1;

        return duration_cast<milliseconds>(m_responseReceivedTime - m_requestSentTime).count();
    }

    int getReceiveTime() const
    {
        if (m_endTime < m_responseReceivedTime || m_responseReceivedTime < m_startTime)
            return -1;

        return duration_cast<milliseconds>(m_endTime - m_responseReceivedTime).count();
    }

    int getTotalTime() const
    {
        if (!hasAnyResponse())
            return -1;

        if (m_endTime < m_startTime)
            return -1;

        return duration_cast<milliseconds>(m_endTime - m_startTime).count();
    }

    bool hasAnyResponse() const
    {
        return m_statusLine.statusCode != 0 || !m_errorText.empty();
    }

    void write();

    QDateTime m_startedDateTime;
    time_point<steady_clock> m_startTime = {};
    time_point<steady_clock> m_connectStartTime = {};
    time_point<steady_clock> m_connectDoneTime = {};
    time_point<steady_clock> m_requestSentTime = {};
    time_point<steady_clock> m_responseReceivedTime = {};
    time_point<steady_clock> m_endTime = {};

    cpptrace::raw_trace m_stackTrace;

    QString m_url;
    nx::network::http::MimeProtoVersion m_requestHttpVersion;
    nx::network::http::Method m_method;

    nx::network::http::HttpHeaders m_requestHeaders;
    nx::Buffer m_requestBody;
    std::optional<std::uint64_t> m_requestBodySize;

    std::string m_serverAddress;
    quint16 m_port = 0;

    StatusLine m_statusLine;
    nx::network::http::HttpHeaders m_responseHeaders;
    nx::Buffer m_responseBody;
    std::optional<std::uint64_t> m_responseBodySize;
    std::optional<std::uint64_t> m_responseContentSize;

    std::string m_errorText;
};

Har::Har(): d(new Private())
{
}

Har::~Har()
{
    NX_MUTEX_LOCKER lock(&d->fileMutex);

    if (!d->harFile.isOpen())
        return;

    d->harFile.close();
    NX_INFO(this, "HAR file: %1", d->harFile.fileName());
}

Har* Har::instance()
{
    static Har instance;
    return &instance;
}

void Har::setFileName(const QString& fileName)
{
    NX_MUTEX_LOCKER lock(&d->fileMutex);

    if (d->harFile.isOpen())
        d->harFile.close();

    d->entryCount = 0;

    if (fileName.isEmpty())
    {
        d->isEnabled = false;
        NX_INFO(this, "HAR logging is disabled.");
        return;
    }

    d->harFile.setFileName(fileName);
    if (!d->harFile.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        NX_WARNING(this, "Failed to open HAR file for writing: %1", fileName);
        return;
    }

    QVariantMap creatorInfo = {
        {"name", qApp->applicationName()},
        {"version", qApp->applicationVersion()}
    };

    QVariantList pagesInfo = {
        QVariantMap{
            {"startedDateTime", currentDateTimeAsString()},
            {"id", "page_0"},
            {"pageTimings", QVariantMap{
                {"onContentLoad", -1},
                {"onLoad", -1}
            }},
            {"title", qApp->applicationName()},
        }
    };

    d->harFile.write("{"); // Start the HAR JSON object.
    d->harFile.write(R"json(
    "log": {
        "version": "1.2",
        "creator": )json");
    d->harFile.write(QJsonDocument::fromVariant(creatorInfo).toJson(QJsonDocument::Compact));
    d->harFile.write(",");
    d->harFile.write(R"json(
        "pages":
            )json");
    d->harFile.write(QJsonDocument::fromVariant(pagesInfo).toJson(QJsonDocument::Compact));
    d->harFile.write(",");
    d->harFile.write(R"json(
        "entries": [
            )json");

    d->harFile.write(kCloseEntries);

    d->flushTimer.start();

    d->isEnabled = true;
}

std::unique_ptr<HarEntry> Har::log()
{
    if (!d->isEnabled)
        return nullptr;

    return std::make_unique<EntryImpl>();
}

void EntryImpl::write()
{
    auto d = Har::instance()->d.get();

    if (m_endTime < m_startTime)
        m_endTime = steady_clock::now();

    NX_MUTEX_LOCKER lock(&d->fileMutex);

    if (!d->harFile.isOpen())
        return;

    d->harFile.seek(d->harFile.pos() - kCloseEntries.size());

    if (d->entryCount > 0)
        d->harFile.write(",\n"); //< Separate entries with a comma.

    ++(d->entryCount);

    QVariantMap entryInfo;

    entryInfo["cache"] = QVariantMap{}; //< For Firefox compatibility.

    entryInfo["startedDateTime"] = dateTimeAsString(m_startedDateTime);

    entryInfo["time"] = getTotalTime();
    entryInfo["serverIPAddress"] = QString::fromStdString(m_serverAddress);
    if (m_port != 0)
    {
        entryInfo["connection"] = (int) m_port;
        entryInfo["_serverPort"] = (int) m_port; //< For Safari compatibility.
    }

    auto requestInfo = QVariantMap{{"method", toString(m_method)},
        {"url", m_url},
        {"httpVersion", toHarHttpVersion(m_requestHttpVersion)},
        {"headers", toHarHeaders(m_requestHeaders)},
        {"queryString", toHarQueryString(m_url)},
        {"cookies", QVariantList{}},
        {"headersSize", -1},
        {"bodySize", (qint64) m_requestBodySize.value_or(m_requestBody.size())}};

    if (!m_requestBody.empty())
    {
        // If no Content-Type header is set, we assume JSON.
        std::string requestContentType = contentType(m_requestHeaders);
        if (requestContentType.empty())
            requestContentType = header::ContentType::kJson.toString();

        if (isTextMediaType(mediaType(requestContentType)))
        {
            requestInfo["postData"] =
                QVariantMap{{"mimeType", QString::fromStdString(requestContentType)},
                    {"text", QString::fromUtf8(m_requestBody)}};
        }
    }

    entryInfo["request"] = requestInfo;

    entryInfo["timings"] = QVariantMap{
        {"blocked", getBlockedTime()},
        {"dns", 0},
        {"connect", getConnectTime()},
        {"send", getSendTime()},
        {"wait", getWaitTime()},
        {"receive", getReceiveTime()},
        {"ssl", -1}
    };

    if (m_statusLine.statusCode != 0)
    {
        const std::string responseContentType = contentType(m_responseHeaders);
        const std::string responseMediaType = mediaType(responseContentType);
        const auto decodedSize = m_responseContentSize.value_or(m_responseBody.size());
        const auto transferredSize = m_responseBodySize.value_or(decodedSize);

        QVariantMap content{{"size", (qint64) decodedSize},
            {"mimeType",
                responseContentType.empty() ? QString("x-unknown")
                                            : QString::fromStdString(responseContentType)}};

        if (transferredSize < decodedSize)
            content["compression"] = (qint64) (decodedSize - transferredSize);

        if (!m_responseBody.empty())
        {
            if (isTextMediaType(responseMediaType))
            {
                content["text"] = QString::fromUtf8(m_responseBody);
            }
            else if (responseMediaType.starts_with("image/"))
            {
                content["text"] = m_responseBody.toBase64().c_str();
                content["encoding"] = "base64";
            }
        }

        entryInfo["response"] = QVariantMap{{"status", m_statusLine.statusCode},
            {"statusText", QString::fromStdString(m_statusLine.reasonPhrase)},
            {"httpVersion", toHarHttpVersion(m_statusLine.version)},
            {"headers", toHarHeaders(m_responseHeaders)},
            {"cookies", QVariantList{}},
            {"content", content},
            {"redirectURL", ""},
            {"headersSize", -1},
            {"bodySize", (qint64) transferredSize},
            {"_error", QString::fromStdString(m_errorText)}};
    }
    else
    {
        entryInfo["response"] = QVariantMap{
            {"status", 0},
            {"statusText", ""},
            {"httpVersion", ""},
            {"headers", QVariantList{}},
            {"cookies", QVariantList{}},
            {"content", QVariantMap{
                {"size", 0},
                {"mimeType", "x-unknown"},
            }},
            {"redirectURL", ""},
            {"headersSize", -1},
            {"bodySize", 0},
            {"_error", QString::fromStdString(m_errorText)}
        };
    }

    if (!m_stackTrace.empty())
    {
        entryInfo["_initiator"] = QVariantMap{
            {"type", "script"},
            {"stack", QVariantMap{
                {"callFrames", getFrameList(m_stackTrace)},
            }}
        };
    }

    d->harFile.write(QJsonDocument::fromVariant(entryInfo).toJson(QJsonDocument::Indented));
    d->harFile.write(kCloseEntries);

    if (milliseconds(d->flushTimer.elapsed()) > kFlushInterval)
    {
        d->harFile.flush();
        d->flushTimer.restart();
    }
}

} // namespace nx::network::http
