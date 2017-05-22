/**********************************************************
* 24 mar 2015
* a.kolesnikov
***********************************************************/

#include <gtest/gtest.h>

#include <nx/utils/match/wildcard.h>


TEST( MatchFunctions, wildcardMatch )
{
    ASSERT_TRUE( wildcardMatch( "*hren?", "odin hrenn" ) );
    ASSERT_FALSE( wildcardMatch( "*hren?", "odin hren" ) );
    ASSERT_TRUE( wildcardMatch( "*", "odin hrenn" ) );
    ASSERT_TRUE( wildcardMatch( "*", "1" ) );
    ASSERT_TRUE( wildcardMatch( "*.json", "file.json" ) );
    ASSERT_FALSE( wildcardMatch( "*.json", "file.xml1" ) );
    ASSERT_FALSE( wildcardMatch( "*.json", "filexml" ) );
    ASSERT_TRUE( wildcardMatch( "*.json", ".json" ) );
    ASSERT_TRUE( wildcardMatch( "*?.json", "file.json" ) );
    ASSERT_FALSE( wildcardMatch( "*?.json", ".json" ) );
    ASSERT_TRUE( wildcardMatch( "resourceData.1.json*", "resourceData.1.json" ) );
    ASSERT_TRUE( wildcardMatch( "abc*def*hren?z??", "abcdefhren1z11" ) );
    ASSERT_FALSE( wildcardMatch( "abc*def*hren?z??", "abcdefhren1z1" ) );
    ASSERT_TRUE( wildcardMatch( "abc*def*hren?z??", "abc123456defhren1z11" ) );
    ASSERT_FALSE( wildcardMatch( "abc*def*hren?z??", "abc123456defhren1z1123" ) );
    ASSERT_TRUE( wildcardMatch( "abc*def*hren?z*?", "abc123456def0000hren1z12321311" ) );

    ASSERT_TRUE(wildcardMatch("/account/*/systems", "/account/pupkin/systems"));
    ASSERT_FALSE(wildcardMatch("/account/*/systems/", "/account/pupkin/systems"));
    ASSERT_FALSE(wildcardMatch("/account/*/systems/", "/account/pupkin/syst"));
}
