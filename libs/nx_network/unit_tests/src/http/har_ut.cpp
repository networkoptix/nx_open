// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <chrono>
#include <future>
#include <memory>
#include <string>
#include <thread>

#include <gtest/gtest.h>

#include <QtCore/QDateTime>
#include <QtCore/QFile>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>

#include <nx/network/http/buffer_source.h>
#include <nx/network/http/http_async_client.h>
#include <nx/network/http/http_stream_reader.h>
#include <nx/network/http/log/har.h>
#include <nx/network/system_socket.h>
#include <nx/network/url/url_builder.h>
#include <nx/utils/gzip/gzip_compressor.h>
#include <nx/utils/std_string_utils.h>
#include <nx/utils/test_support/test_with_temporary_directory.h>

namespace nx::network::http::test {

namespace {

using namespace std::chrono_literals;

constexpr std::chrono::milliseconds kSocketTimeout = 10s;

const std::string kSoapContentType =
    "application/soap+xml; charset=utf-8; "
    R"(action="http://www.onvif.org/ver10/device/wsdl/GetSystemDateAndTime")";
const nx::Buffer kSoapRequest = R"(<?xml version="1.0" encoding="UTF-8"?>)"
                                "<Envelope><Body><GetSystemDateAndTime/></Body></Envelope>";
const nx::Buffer kSoapResponse =
    R"(<?xml version="1.0" encoding="UTF-8"?>)"
    "<Envelope><Body><GetSystemDateAndTimeResponse/></Body></Envelope>";

QString str(const std::string& value)
{
    return QString::fromStdString(value);
}

QString str(const nx::Buffer& value)
{
    return QString::fromUtf8(value.data(), (qsizetype) value.size());
}

QJsonValue findHeader(const QJsonObject& message, const QString& name)
{
    for (const QJsonValue header: message["headers"].toArray())
    {
        if (header["name"].toString().compare(name, Qt::CaseInsensitive) == 0)
            return header["value"];
    }
    return QJsonValue::Undefined;
}

/**
 * Accepts a single connection, reads a single request from it and replies with the given bytes.
 * Every socket operation is limited by a timeout, so the server thread always ends.
 */
class OneShotHttpServer
{
public:
    explicit OneShotHttpServer(nx::Buffer response):
        m_server(std::make_unique<TCPServerSocket>(SocketFactory::tcpClientIpVersion()))
    {
        NX_ASSERT(m_server->bind(SocketAddress::anyPrivateAddress));
        NX_ASSERT(m_server->setRecvTimeout(kSocketTimeout.count())); //< Limits accept().
        NX_ASSERT(m_server->listen());

        m_thread = std::thread(
            [this, response = std::move(response)]()
            {
                const auto connection = m_server->accept();
                if (!connection || !connection->setRecvTimeout(kSocketTimeout)
                    || !connection->setSendTimeout(kSocketTimeout))
                {
                    return;
                }

                HttpStreamReader reader;
                while (reader.state() != HttpStreamReader::ReadState::messageDone)
                {
                    nx::Buffer buffer(4096, 0);
                    const int bytesRead = connection->recv(buffer.data(), buffer.size());
                    if (bytesRead <= 0)
                        return;
                    if (!reader.parseBytes({buffer.data(), (size_t) bytesRead}))
                        return;
                }

                connection->send(response.data(), response.size());
            });
    }

    ~OneShotHttpServer() { m_thread.join(); }

    SocketAddress address() const { return m_server->getLocalAddress(); }

private:
    std::unique_ptr<TCPServerSocket> m_server;
    std::thread m_thread;
};

} // namespace

class HarLog: public ::testing::Test, public nx::utils::test::TestWithTemporaryDirectory
{
protected:
    virtual void SetUp() override
    {
        m_fileName = testDataDir() + "/test.har";
        http::Har::instance()->setFileName(m_fileName);
    }

    virtual void TearDown() override { http::Har::instance()->setFileName(QString()); }

    /** Closes HAR file (which disables logging) and returns its "log" object. */
    const QJsonObject closeAndReadLog()
    {
        http::Har::instance()->setFileName(QString());

        QFile file(m_fileName);
        if (!file.open(QIODevice::ReadOnly))
        {
            ADD_FAILURE() << "Cannot open " << m_fileName.toStdString();
            return {};
        }

        const QByteArray content = file.readAll();
        QJsonParseError error;
        const auto document = QJsonDocument::fromJson(content, &error);
        EXPECT_EQ(QJsonParseError::NoError, error.error)
            << error.errorString().toStdString() << " at " << error.offset << ":\n"
            << content.toStdString();
        return document.object()["log"].toObject();
    }

    QJsonArray closeAndReadEntries() { return closeAndReadLog()["entries"].toArray(); }

    const QJsonObject closeAndReadSingleEntry()
    {
        const auto entries = closeAndReadEntries();
        EXPECT_EQ(1, entries.size());
        return entries.empty() ? QJsonObject() : entries.first().toObject();
    }

    static std::unique_ptr<HarEntry> createEntry(const Method& method = Method::post,
        const QString& url = "http://127.0.0.1/onvif/device_service",
        const HttpHeaders& headers = {})
    {
        auto entry = http::Har::instance()->log();
        if (entry)
            entry->setRequest(method, url, http_1_1, headers);
        return entry;
    }

    static StatusLine statusLine(
        StatusCode::Value statusCode = StatusCode::ok, const MimeProtoVersion& version = http_1_1)
    {
        StatusLine result;
        result.version = version;
        result.statusCode = statusCode;
        result.reasonPhrase = StatusCode::toString(statusCode);
        return result;
    }

    /** Response with the gzip-compressed body, as served by OneShotHttpServer. */
    static nx::Buffer gzipResponse(const nx::Buffer& body, const std::string& contentType)
    {
        const nx::Buffer compressedBody = nx::utils::bstream::gzip::Compressor::compressData(body);

        Response response;
        response.statusLine = statusLine();
        response.headers = {{header::ContentType::NAME, contentType},
            {"Content-Encoding", "gzip"},
            {"Connection", "close"},
            {header::kContentLength, std::to_string(compressedBody.size())}};
        response.messageBody = compressedBody;

        nx::Buffer result;
        response.serialize(&result);
        return result;
    }

private:
    QString m_fileName;
};

TEST_F(HarLog, isDisabledWithoutFile)
{
    http::Har::instance()->setFileName(QString());
    EXPECT_EQ(nullptr, http::Har::instance()->log());
}

TEST_F(HarLog, fileWithoutEntriesIsValid)
{
    const auto log = closeAndReadLog();
    EXPECT_EQ("1.2", log["version"].toString());
    EXPECT_TRUE(log["entries"].isArray());
    EXPECT_TRUE(log["entries"].toArray().empty());
}

TEST_F(HarLog, entriesAreAppended)
{
    for (int i = 0; i < 3; ++i)
    {
        auto entry = createEntry(Method::get, QString("http://127.0.0.1/%1").arg(i));
        ASSERT_NE(nullptr, entry);
        entry->setResponseHeaders(statusLine(), {});
    }

    const auto entries = closeAndReadEntries();
    ASSERT_EQ(3, entries.size());
    for (int i = 0; i < 3; ++i)
    {
        EXPECT_EQ(QString("http://127.0.0.1/%1").arg(i), entries[i]["request"]["url"].toString());
    }
}

TEST_F(HarLog, startedDateTimeIsTheRequestStart)
{
    const auto beforeStart = QDateTime::currentDateTime().addMSecs(-1);
    auto entry = createEntry();
    ASSERT_NE(nullptr, entry);
    const auto afterStart = QDateTime::currentDateTime().addMSecs(1);

    std::this_thread::sleep_for(100ms);
    entry->setResponseHeaders(statusLine(), {});
    entry.reset();

    const auto startedDateTime = QDateTime::fromString(
        closeAndReadSingleEntry()["startedDateTime"].toString(), Qt::ISODateWithMs);
    ASSERT_TRUE(startedDateTime.isValid());
    EXPECT_GE(startedDateTime, beforeStart);
    EXPECT_LE(startedDateTime, afterStart);
}

TEST_F(HarLog, soapBodiesAreLogged)
{
    auto entry = createEntry(Method::post,
        "http://127.0.0.1/onvif/device_service",
        {{header::ContentType::NAME, kSoapContentType}});
    ASSERT_NE(nullptr, entry);
    entry->setRequestBody(kSoapRequest);
    entry->setResponseHeaders(
        statusLine(), {{header::ContentType::NAME, "application/soap+xml; charset=utf-8"}});
    entry->setResponseBody(kSoapResponse);
    entry.reset();

    const auto harEntry = closeAndReadSingleEntry();

    const auto request = harEntry["request"].toObject();
    EXPECT_EQ(str(kSoapContentType), request["postData"]["mimeType"].toString());
    EXPECT_EQ(str(kSoapRequest), request["postData"]["text"].toString());
    EXPECT_EQ((qint64) kSoapRequest.size(), request["bodySize"].toInteger());

    const auto content = harEntry["response"]["content"].toObject();
    EXPECT_EQ("application/soap+xml; charset=utf-8", content["mimeType"].toString());
    EXPECT_EQ(str(kSoapResponse), content["text"].toString());
    EXPECT_EQ((qint64) kSoapResponse.size(), content["size"].toInteger());
    EXPECT_FALSE(content.contains("compression"));
}

TEST_F(HarLog, contentTypeParametersAndCaseAreIgnored)
{
    const nx::Buffer kJson = R"({"a":1})";

    auto entry = createEntry(Method::get, "http://127.0.0.1/rest/v4/system/info");
    ASSERT_NE(nullptr, entry);
    entry->setResponseHeaders(
        statusLine(), {{header::ContentType::NAME, "Application/JSON ; charset=UTF-8"}});
    entry->setResponseBody(kJson);
    entry.reset();

    EXPECT_EQ(str(kJson), closeAndReadSingleEntry()["response"]["content"]["text"].toString());
}

TEST_F(HarLog, requestBodyIsLoggedForAnyMethod)
{
    const nx::Buffer kJson = R"({"name":"camera"})";

    auto entry = createEntry(Method::patch,
        "http://127.0.0.1/rest/v4/devices/1",
        {{header::ContentType::NAME, header::ContentType::kJson.toString()}});
    ASSERT_NE(nullptr, entry);
    entry->setRequestBody(kJson);
    entry->setResponseHeaders(statusLine(), {});
    entry.reset();

    const auto request = closeAndReadSingleEntry()["request"].toObject();
    EXPECT_EQ("PATCH", request["method"].toString());
    EXPECT_EQ(str(kJson), request["postData"]["text"].toString());
}

TEST_F(HarLog, requestBodyWithoutContentTypeIsAssumedJson)
{
    const nx::Buffer kJson = R"({"a":1})";

    auto entry = createEntry();
    ASSERT_NE(nullptr, entry);
    entry->setRequestBody(kJson);
    entry->setResponseHeaders(statusLine(), {});
    entry.reset();

    const auto postData = closeAndReadSingleEntry()["request"]["postData"].toObject();
    EXPECT_EQ(str(header::ContentType::kJson.toString()), postData["mimeType"].toString());
    EXPECT_EQ(str(kJson), postData["text"].toString());
}

TEST_F(HarLog, binaryBodiesAreNotLoggedAsText)
{
    const nx::Buffer kBinary("\x00\x01\x02\x03", 4);

    auto entry = createEntry(Method::post,
        "http://127.0.0.1/upload",
        {{header::ContentType::NAME, header::ContentType::kBinary.toString()}});
    ASSERT_NE(nullptr, entry);
    entry->setRequestBody(kBinary);
    entry->setResponseHeaders(
        statusLine(), {{header::ContentType::NAME, header::ContentType::kBinary.toString()}});
    entry->setResponseBody(kBinary);
    entry.reset();

    const auto harEntry = closeAndReadSingleEntry();
    EXPECT_FALSE(harEntry["request"].toObject().contains("postData"));
    EXPECT_EQ(4, harEntry["request"]["bodySize"].toInteger());

    const auto content = harEntry["response"]["content"].toObject();
    EXPECT_FALSE(content.contains("text"));
    EXPECT_EQ(str(header::ContentType::kBinary.toString()), content["mimeType"].toString());
    EXPECT_EQ(4, content["size"].toInteger());
}

TEST_F(HarLog, imageBodyIsLoggedAsBase64)
{
    const nx::Buffer kImage("\xFF\xD8\xFF\xE0", 4);

    auto entry = createEntry(Method::get, "http://127.0.0.1/snapshot.jpg");
    ASSERT_NE(nullptr, entry);
    entry->setResponseHeaders(statusLine(), {{header::ContentType::NAME, "image/jpeg"}});
    entry->setResponseBody(kImage);
    entry.reset();

    const auto content = closeAndReadSingleEntry()["response"]["content"].toObject();
    EXPECT_EQ("base64", content["encoding"].toString());
    EXPECT_EQ(str(kImage.toBase64()), content["text"].toString());
}

TEST_F(HarLog, transferredSizesAreReported)
{
    const nx::Buffer kBody(1000, 'a');

    auto entry = createEntry(Method::post,
        "http://127.0.0.1/onvif/device_service",
        {{header::ContentType::NAME, kSoapContentType}, {"Content-Encoding", "gzip"}});
    ASSERT_NE(nullptr, entry);
    entry->setRequestBody(kBody);
    entry->setRequestBodySize(30);
    entry->setResponseHeaders(statusLine(),
        {{header::ContentType::NAME, header::ContentType::kPlain.toString()},
            {"Content-Encoding", "gzip"}});
    entry->setResponseBody(kBody);
    entry->setResponseBodySize(40);
    entry.reset();

    const auto harEntry = closeAndReadSingleEntry();
    EXPECT_EQ(30, harEntry["request"]["bodySize"].toInteger());
    EXPECT_EQ(str(kBody), harEntry["request"]["postData"]["text"].toString());

    const auto response = harEntry["response"].toObject();
    EXPECT_EQ(40, response["bodySize"].toInteger());
    EXPECT_EQ(1000, response["content"]["size"].toInteger());
    EXPECT_EQ(960, response["content"]["compression"].toInteger());
    EXPECT_EQ(str(kBody), response["content"]["text"].toString());
}

// The client may have already fetched the body, so only its last part is passed to the entry.
TEST_F(HarLog, contentSizeIsReportedForPartiallyLoggedBody)
{
    auto entry = createEntry(Method::get, "http://127.0.0.1/rest/v4/devices");
    ASSERT_NE(nullptr, entry);
    entry->setResponseHeaders(statusLine(),
        {{header::ContentType::NAME, header::ContentType::kJson.toString()},
            {"Content-Encoding", "gzip"}});
    entry->setResponseBody(nx::Buffer(100, 'a'));
    entry->setResponseBodySize(8192);
    entry->setResponseContentSize(51200);
    entry.reset();

    const auto response = closeAndReadSingleEntry()["response"].toObject();
    EXPECT_EQ(8192, response["bodySize"].toInteger());
    EXPECT_EQ(51200, response["content"]["size"].toInteger());
    EXPECT_EQ(43008, response["content"]["compression"].toInteger());
}

TEST_F(HarLog, httpVersion)
{
    auto entry = createEntry(Method::get);
    ASSERT_NE(nullptr, entry);
    entry->setResponseHeaders(statusLine(StatusCode::ok, http_1_0), {});
    entry.reset();

    EXPECT_EQ("HTTP/1.1", closeAndReadSingleEntry()["request"]["httpVersion"].toString());
    EXPECT_EQ("HTTP/1.0", closeAndReadSingleEntry()["response"]["httpVersion"].toString());
}

TEST_F(HarLog, queryStringIsParsedFromUrl)
{
    auto entry = createEntry(Method::get, "http://127.0.0.1/path?a=1&b=x%20y");
    ASSERT_NE(nullptr, entry);
    entry->setResponseHeaders(statusLine(), {});
    entry.reset();

    const auto queryString = closeAndReadSingleEntry()["request"]["queryString"].toArray();
    ASSERT_EQ(2, queryString.size());
    EXPECT_EQ("a", queryString[0]["name"].toString());
    EXPECT_EQ("1", queryString[0]["value"].toString());
    EXPECT_EQ("b", queryString[1]["name"].toString());
    EXPECT_EQ("x y", queryString[1]["value"].toString());
}

TEST_F(HarLog, timingsAreReportedForReusedConnection)
{
    auto entry = createEntry(Method::get);
    ASSERT_NE(nullptr, entry);
    entry->markRequestSent(); //< No connect marks: the request goes over a reused connection.
    entry->setResponseHeaders(statusLine(), {});
    entry->setResponseBody({});
    entry.reset();

    const auto timings = closeAndReadSingleEntry()["timings"].toObject();
    EXPECT_EQ(-1, timings["connect"].toInt());
    EXPECT_GE(timings["send"].toInt(), 0);
    EXPECT_GE(timings["wait"].toInt(), 0);
    EXPECT_GE(timings["receive"].toInt(), 0);
}

TEST_F(HarLog, failedRequestHasError)
{
    auto entry = createEntry();
    ASSERT_NE(nullptr, entry);
    entry->setError("Connection refused");
    entry.reset();

    const auto response = closeAndReadSingleEntry()["response"].toObject();
    EXPECT_EQ(0, response["status"].toInt());
    EXPECT_EQ("Connection refused", response["_error"].toString());
}

TEST_F(HarLog, requestWithoutResponseIsReportedAsDropped)
{
    auto entry = createEntry();
    ASSERT_NE(nullptr, entry);
    entry.reset();

    const auto response = closeAndReadSingleEntry()["response"].toObject();
    EXPECT_EQ(0, response["status"].toInt());
    EXPECT_EQ("Dropped request", response["_error"].toString());
}

TEST_F(HarLog, asyncClientExchangeIsLogged)
{
    const nx::Buffer kRequestBody = R"({"name":"camera"})";
    const nx::Buffer kResponseBody(500, 'x');
    const nx::Buffer kCompressedBody =
        nx::utils::bstream::gzip::Compressor::compressData(kResponseBody);

    OneShotHttpServer server(gzipResponse(kResponseBody, header::ContentType::kPlain.toString()));

    const auto url = url::Builder()
                         .setScheme(kUrlSchemeName)
                         .setEndpoint(server.address())
                         .setPath("/rest/v4/devices/1")
                         .setQuery("a=1")
                         .toUrl();

    AsyncClient client(ssl::kAcceptAnyCertificate);
    std::promise<void> done;
    client.doPatch(url,
        std::make_unique<BufferSource>(header::ContentType::kJson.toString(), kRequestBody),
        [&done]() { done.set_value(); });
    done.get_future().wait();
    client.pleaseStopSync();

    ASSERT_FALSE(client.failed());
    ASSERT_EQ(kResponseBody, client.fetchMessageBodyBuffer());

    const auto harEntry = closeAndReadSingleEntry();

    EXPECT_EQ(str(server.address().address.toString()), harEntry["serverIPAddress"].toString());
    EXPECT_EQ(server.address().port, harEntry["connection"].toInt());
    EXPECT_GE(harEntry["time"].toInt(), 0);
    EXPECT_GE(harEntry["timings"]["connect"].toInt(), 0);
    EXPECT_GE(harEntry["timings"]["wait"].toInt(), 0);

    const auto request = harEntry["request"].toObject();
    EXPECT_EQ("PATCH", request["method"].toString());
    EXPECT_EQ(url.toString(), request["url"].toString());
    EXPECT_EQ(str(kRequestBody), request["postData"]["text"].toString());
    EXPECT_EQ(1, request["queryString"].toArray().size());

    const auto harResponse = harEntry["response"].toObject();
    EXPECT_EQ(200, harResponse["status"].toInt());
    EXPECT_EQ("gzip", findHeader(harResponse, "Content-Encoding").toString());
    EXPECT_EQ((qint64) kCompressedBody.size(), harResponse["bodySize"].toInteger());
    EXPECT_EQ((qint64) kResponseBody.size(), harResponse["content"]["size"].toInteger());
    EXPECT_EQ(str(kResponseBody), harResponse["content"]["text"].toString());
}

// Clients like HttpClient and AsyncFileDownloader fetch the body as it arrives, leaving nothing
// in the client buffer at the message end.
TEST_F(HarLog, asyncClientStreamedResponseSizesAreReported)
{
    std::string json = "[";
    for (int i = 0; i < 5000; ++i)
        json += nx::utils::buildString(i == 0 ? "" : ",", R"({"id":)", i, "}");
    json += "]";
    const nx::Buffer kResponseBody(json);
    const nx::Buffer kCompressedBody =
        nx::utils::bstream::gzip::Compressor::compressData(kResponseBody);

    OneShotHttpServer server(gzipResponse(kResponseBody, header::ContentType::kJson.toString()));

    AsyncClient client(ssl::kAcceptAnyCertificate);
    nx::Buffer fetchedBody;
    client.setOnSomeMessageBodyAvailable(
        [&client, &fetchedBody]() { fetchedBody += client.fetchMessageBodyBuffer(); });
    std::promise<void> done;
    client.doGet(url::Builder().setScheme(kUrlSchemeName).setEndpoint(server.address()).toUrl(),
        [&done]() { done.set_value(); });
    done.get_future().wait();
    client.pleaseStopSync();

    ASSERT_FALSE(client.failed());
    fetchedBody += client.fetchMessageBodyBuffer();
    ASSERT_EQ(kResponseBody, fetchedBody);

    const auto response = closeAndReadSingleEntry()["response"].toObject();
    EXPECT_EQ((qint64) kCompressedBody.size(), response["bodySize"].toInteger());
    EXPECT_EQ((qint64) kResponseBody.size(), response["content"]["size"].toInteger());
    EXPECT_EQ((qint64) (kResponseBody.size() - kCompressedBody.size()),
        response["content"]["compression"].toInteger());
}

} // namespace nx::network::http::test
