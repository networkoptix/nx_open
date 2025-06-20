// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "har.h"

#include <chrono>

#include <cpptrace/cpptrace.hpp>

#include <QtCore/QCoreApplication>
#include <QtCore/QDateTime>
#include <QtCore/QDir>
#include <QtCore/QJsonDocument>
#include <QtCore/QString>

#include <nx/branding.h>
#include <nx/build_info.h>
#include <nx/kit/ini_config.h>
#include <nx/utils/log/log.h>
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

static QString currentDateTimeAsString()
{
    QDateTime local(QDateTime::currentDateTime());
    local.setTimeZone(QTimeZone::systemTimeZone());
    return local.toString(Qt::ISODateWithMs);
}

} // namespace

namespace nx::network::http {

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

    void setRequest(
        nx::network::http::Method method,
        const QString& url,
        const nx::network::http::HttpHeaders& headers) override
    {
        m_method = method;
        m_url = url;
        m_requestHeaders = headers;
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

    void setResponseBody(const nx::Buffer& body) override
    {
        m_responseBody = body;
        m_endTime = steady_clock::now();
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
        if (m_requestSentTime < m_connectDoneTime || m_connectDoneTime < m_startTime)
            return -1;

        return duration_cast<milliseconds>(m_requestSentTime - m_connectDoneTime).count();
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
    time_point<steady_clock> m_responseEndTime = {};
    time_point<steady_clock> m_endTime = {};

    cpptrace::raw_trace m_stackTrace;

    QString m_url;

    nx::network::http::Method m_method;

    nx::network::http::HttpHeaders m_requestHeaders;
    nx::Buffer m_requestBody;

    std::string m_serverAddress;
    quint16 m_port = 0;

    StatusLine m_statusLine;
    nx::network::http::HttpHeaders m_responseHeaders;
    nx::Buffer m_responseBody;

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

    QVariantList requestHeaders;
    for (const auto& header: m_requestHeaders)
    {
        requestHeaders.append(QVariantMap{
            {"name", QString::fromStdString(header.first)},
            {"value", QString::fromStdString(header.second)}
        });
    }

    entryInfo["startedDateTime"] = currentDateTimeAsString();

    entryInfo["time"] = getTotalTime();
    entryInfo["serverIPAddress"] = QString::fromStdString(m_serverAddress);
    if (m_port != 0)
    {
        entryInfo["connection"] = (int) m_port;
        entryInfo["_serverPort"] = (int) m_port; //< For Safari compatibility.
    }

    auto requestInfo = QVariantMap{
        {"method", toString(m_method)},
        {"url", m_url},
        {"httpVersion", "HTTP/1.1"},
        {"headers", requestHeaders},
        {"queryString", QVariantList{}},
        {"cookies", QVariantList{}},
        {"headersSize", -1},
        {"bodySize", 0}
    };

    if ((m_method == nx::network::http::Method::post || m_method == nx::network::http::Method::put)
        && !m_requestBody.empty())
    {
        auto it = m_requestHeaders.find("Content-Type");
        if (it == m_requestHeaders.end() || it->second == "application/json")
        {
            // If no Content-Type header is set, we assume JSON.

            requestInfo["bodySize"] = QVariant((int) m_requestBody.size());
            requestInfo["postData"] = QVariantMap{
                {"mimeType", "application/json"}, // Placeholder for content type.
                {"text", QString::fromUtf8(m_requestBody)}
            };
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
        QVariantList responseHeaders;
        for (const auto& header: m_responseHeaders)
        {
            responseHeaders.append(QVariantMap{
                {"name", QString::fromStdString(header.first)},
                {"value", QString::fromStdString(header.second)}
            });
        }

        QVariantMap responseInfo = {
            {"status", m_statusLine.statusCode},
            {"statusText", QString::fromStdString(m_statusLine.reasonPhrase)},
            {"httpVersion", "HTTP/1.1"},
            {"headers", responseHeaders},
            {"cookies", QVariantList{}},
            {"content", QVariantMap{
                {"size", 0},
                {"mimeType", "application/json"},
                {"text", ""}
            }},
            {"redirectURL", ""},
            {"headersSize", -1},
            {"bodySize", 0},
            {"_error", QString::fromStdString(m_errorText)}
        };

        if (!m_responseBody.empty())
        {
            if (const auto it = m_responseHeaders.find("Content-Type");
                it != m_responseHeaders.end())
            {
                if (it->second == "application/json"
                    || it->second.starts_with("application/xml")
                    || it->second.starts_with("text/"))
                {
                    responseInfo["content"] = QVariantMap{
                        {"size", (int) m_responseBody.size()},
                        {"mimeType", QString::fromStdString(it->second)},
                        {"text", QString::fromUtf8(m_responseBody)}
                    };
                }
                else if (it->second.starts_with("image/"))
                {
                    responseInfo["content"] = QVariantMap{
                        {"size", (int) m_responseBody.size()},
                        {"mimeType", QString::fromStdString(it->second)},
                        {"text", m_responseBody.toBase64().c_str()},
                        {"encoding", "base64"}
                    };
                }
            }
        }

        entryInfo["response"] = responseInfo;
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
