#include <gtest/gtest.h>

#include <nx/utils/match/wildcard.h>

TEST(MatchWildcard, case_sensitive)
{
    ASSERT_TRUE(wildcardMatch("*hren?", "odin hrenn"));
    ASSERT_FALSE(wildcardMatch("*hren?", "odin hren"));
    ASSERT_TRUE(wildcardMatch("*", "odin hrenn"));
    ASSERT_TRUE(wildcardMatch("*", "1"));
    ASSERT_TRUE(wildcardMatch("*.json", "file.json"));
    ASSERT_FALSE(wildcardMatch("*.json", "file.xml1"));
    ASSERT_FALSE(wildcardMatch("*.json", "filexml"));
    ASSERT_TRUE(wildcardMatch("*.json", ".json"));
    ASSERT_TRUE(wildcardMatch("*?.json", "file.json"));
    ASSERT_FALSE(wildcardMatch("*?.json", ".json"));
    ASSERT_TRUE(wildcardMatch("resourceData.1.json*", "resourceData.1.json"));
    ASSERT_TRUE(wildcardMatch("abc*def*hren?z??", "abcdefhren1z11"));
    ASSERT_FALSE(wildcardMatch("abc*def*hren?z??", "abcdefhren1z1"));
    ASSERT_TRUE(wildcardMatch("abc*def*hren?z??", "abc123456defhren1z11"));
    ASSERT_FALSE(wildcardMatch("abc*def*hren?z??", "abc123456defhren1z1123"));
    ASSERT_TRUE(wildcardMatch("abc*def*hren?z*?", "abc123456def0000hren1z12321311"));

    ASSERT_TRUE(wildcardMatch("/account/*/systems", "/account/pupkin/systems"));
    ASSERT_FALSE(wildcardMatch("/account/*/systems/", "/account/pupkin/systems"));
    ASSERT_FALSE(wildcardMatch("/account/*/systems/", "/account/pupkin/syst"));
}

TEST(MatchWildcard, case_insensitive)
{
    const auto mode = MatchMode::caseInsensitive;

    ASSERT_TRUE(wildcardMatch("abC*def*Hren?z??", "aBcdefhRen1Z11", mode));
    ASSERT_FALSE(wildcardMatch("/account/*/systems/", "/account/pupkin/systems", mode));
}

TEST(MatchWildcard, QString_is_supported)
{
    ASSERT_TRUE(wildcardMatch(
        QString("abC*def*Hren?z??"),
        QString("aBcdefhRen1Z11"),
        MatchMode::caseInsensitive));

    ASSERT_FALSE(wildcardMatch(
        QString("/account/*/systems/"),
        QString("/account/pupkin/systems"),
        MatchMode::caseInsensitive));

    ASSERT_TRUE(wildcardMatch(QString("abc*def*hren?z??"), QString("abc123456defhren1z11")));
    ASSERT_FALSE(wildcardMatch(QString("abC*def*Hren?z??"), QString("aBcdefhRen1Z11")));
}
