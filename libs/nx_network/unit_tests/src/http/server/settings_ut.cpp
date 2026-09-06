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
    addArg("-http/extraSuccessResponseHeaders/server", compatibilityServerName());
    addArg("-http/extraSuccessResponseHeaders/other", "other");

    loadSettings();

    ASSERT_EQ(
        compatibilityServerName(),
        httpServerSettings().extraSuccessResponseHeaders.find("server")->second);

    ASSERT_EQ(
        "other",
        httpServerSettings().extraSuccessResponseHeaders.find("other")->second);
}

// ANAS-323: request body size must be capped by default, and configurable per service.

TEST_F(HttpServerSettings, maxMessageBodySize_defaults_to_a_safe_non_zero_value)
{
    loadSettings();

    ASSERT_EQ(Settings::kDefaultMaxMessageBodySize, httpServerSettings().maxMessageBodySize);
}

TEST_F(HttpServerSettings, load_maxMessageBodySize)
{
    addArg("-http/maxMessageBodySize", "12345");

    loadSettings();

    ASSERT_EQ(std::uint64_t{12345}, httpServerSettings().maxMessageBodySize);
}

TEST_F(HttpServerSettings, maxHeadersSize_defaults_to_a_safe_non_zero_value)
{
    loadSettings();

    ASSERT_EQ(Settings::kDefaultMaxHeadersSize, httpServerSettings().maxHeadersSize);
}

TEST_F(HttpServerSettings, load_maxHeadersSize)
{
    addArg("-http/maxHeadersSize", "4096");

    loadSettings();

    ASSERT_EQ(std::uint64_t{4096}, httpServerSettings().maxHeadersSize);
}

} // namespace nx::network::http::server::test
