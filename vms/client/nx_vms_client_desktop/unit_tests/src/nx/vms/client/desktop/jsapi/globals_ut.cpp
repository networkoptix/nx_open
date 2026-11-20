// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <QtCore/QUrl>
#include <QtGui/QDesktopServices>

#include <nx/vms/client/desktop/jsapi/globals.h>

namespace nx::vms::client::desktop::test {

class UrlHandler: public QObject
{
    Q_OBJECT

public:
    bool opened = false;

public slots:
    void openUrl(const QUrl& /*url*/) { opened = true; }
};

class JsApiGlobalsTest: public ::testing::Test
{
protected:
    virtual void SetUp() override
    {
        for (const auto& scheme: kSchemes)
            QDesktopServices::setUrlHandler(scheme, &handler, "openUrl");
    }

    virtual void TearDown() override
    {
        for (const auto& scheme: kSchemes)
            QDesktopServices::unsetUrlHandler(scheme);
    }

    bool opens(const QString& url)
    {
        handler.opened = false;
        globals.openUrlExternally(url);
        return handler.opened;
    }

    // Refused and empty schemes are intercepted too, so a leaked URL is not opened by the OS.
    static inline const QStringList kSchemes{
        "", "http", "https", "mailto", "tel", "file", "smb", "ftp", "javascript", "custom"};

    UrlHandler handler;
    jsapi::Globals globals;
};

TEST_F(JsApiGlobalsTest, allowedUrlsAreOpened)
{
    EXPECT_TRUE(opens("http://example.com"));
    EXPECT_TRUE(opens("https://example.com:8443/path?query=1#fragment"));
    EXPECT_TRUE(opens("HTTPS://example.com"));
    EXPECT_TRUE(opens("mailto:user@example.com?subject=Test"));
    EXPECT_TRUE(opens("tel:+1-234-567-890"));
}

TEST_F(JsApiGlobalsTest, otherUrlsAreRefused)
{
    EXPECT_FALSE(opens("notepad"));
    EXPECT_FALSE(opens("cmd"));
    EXPECT_FALSE(opens("http:example.com"));
    EXPECT_FALSE(opens("http:///path"));
    EXPECT_FALSE(opens("file:///etc/passwd"));
    EXPECT_FALSE(opens("smb://example.com/share"));
    EXPECT_FALSE(opens("ftp://example.com"));
    EXPECT_FALSE(opens("javascript:alert(1)"));
    EXPECT_FALSE(opens("custom://example.com"));
}

} // namespace nx::vms::client::desktop::test

#include "globals_ut.moc"
