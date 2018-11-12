#include <gtest/gtest.h>

#include <nx/fusion/serialization/lexical.h>
#include <nx/fusion/serialization/lexical_functions.h>

TEST( QnLexical, Bool )
{
    EXPECT_EQ( QnLexical::serialized(true), QLatin1String("true") );
    EXPECT_EQ( QnLexical::serialized(false), QLatin1String("false") );

    EXPECT_EQ( QnLexical::deserialized<bool>(QLatin1String("true")), true );
    EXPECT_EQ( QnLexical::deserialized<bool>(QLatin1String("True")), true );
    EXPECT_EQ( QnLexical::deserialized<bool>(QLatin1String("1")), true );

    EXPECT_EQ( QnLexical::deserialized<bool>(QLatin1String("false")), false );
    EXPECT_EQ( QnLexical::deserialized<bool>(QLatin1String("False")), false );
    EXPECT_EQ( QnLexical::deserialized<bool>(QLatin1String("0")), false );

    EXPECT_EQ( QnLexical::deserialized<bool>(QLatin1String("hello")), false );
    EXPECT_EQ( QnLexical::deserialized<bool>(QLatin1String("3")), false );

    EXPECT_EQ( QnLexical::deserialized(QLatin1String("true"), false), true );
    EXPECT_EQ( QnLexical::deserialized(QLatin1String("false"), true), false );
    EXPECT_EQ( QnLexical::deserialized(QLatin1String("kk"), true), true );
    EXPECT_EQ( QnLexical::deserialized(QLatin1String(""), true), true );

    EXPECT_TRUE( QnLexical::deserialized<bool>( QnLexical::serialized(true) ) );
    EXPECT_FALSE( QnLexical::deserialized<bool>( QnLexical::serialized(false) ) );
}
