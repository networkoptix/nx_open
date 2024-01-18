// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <string_view>

#include <nx/network/http/server/settings.h>
#include <nx/utils/test_support/deprecated_settings.h>

namespace nx::network::http::server::test {

class HttpServerSettings: public ::testing::Test, public nx::utils::test::TestSettingsReader
{
public:
    void loadSettings()
    {
        m_httpServerSettings.load(*this);
    }

    const server::Settings& httpServerSettings() { return m_httpServerSettings; }

private:
    server::Settings m_httpServerSettings;
};

TEST_F(HttpServerSettings, load_extraResponseHeaders)
{
    addArg("-http/extraResponseHeaders/server", compatibilityServerName());
    addArg("-http/extraResponseHeaders/other", "other");

    loadSettings();

    ASSERT_EQ(
        compatibilityServerName(),
        httpServerSettings().extraResponseHeaders.find("server")->second);

    ASSERT_EQ(
        "other",
        httpServerSettings().extraResponseHeaders.find("other")->second);
}

} // namespace nx::network::http::server::test
