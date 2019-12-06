#include <http/auto_request_forwarder.h>

#include <gtest/gtest.h>

TEST(Log, HidePassword)
{
    QByteArray originalLine(
        "GET /api/storageStatus/?X-server-guid={67512e53-94af-d423-f652-226cd204"
        "ba91}&path=smb://anonymous:qwe@host/path HTTP/1.1");

    QByteArray originalLineHalfEncoded(
        "GET /api/storageStatus/?X-server-guid={67512e53-94af-d423-f652-226cd204"
        "ba91}&path=smb%3A%2F%2Fanonymous%3Aqwe%4010.1.5.200%2Fakulikov HTTP/1.1");

    for (const auto& line: {originalLine, originalLineHalfEncoded})
    {
        nx::network::http::RequestLine requestLine;
        ASSERT_TRUE(requestLine.parse(line));
        ASSERT_FALSE(detail::hidePassword(requestLine).contains("qwe"));
    }
}