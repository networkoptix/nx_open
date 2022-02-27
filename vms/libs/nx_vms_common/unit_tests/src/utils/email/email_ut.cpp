// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <utils/email/email.h>

namespace {

static const QString kUser("user");
static const QString kDomain("domain.net");
static const QString kSuffix("suffix");
static const QString kName("U. Name");

static const QString kEmail("user@domain.net");
static const QString kEmailWithName("U. Name <user@domain.net>");
static const QString kEmailWithSuffix("user+suffix@domain.net");
static const QString kEmailWithNameAndSuffix("U. Name <user+suffix@domain.net>");

} // namespace

namespace nx::email {
namespace test {

TEST(QnEmail, emptyStringIsNotValidEmail)
{
    ASSERT_FALSE(isValidAddress(""));
}

TEST(QnEmail, ValidAddr)
{
    ASSERT_TRUE(isValidAddress(kEmail));
}

TEST(QnEmail, ValidFullName)
{
    ASSERT_TRUE(isValidAddress(kEmailWithName));
}

TEST(QnEmail, ValidSuffix)
{
    ASSERT_TRUE(isValidAddress(kEmailWithSuffix));
}

TEST(QnEmail, ValidNameAndSuffix)
{
    ASSERT_TRUE(isValidAddress(kEmailWithNameAndSuffix));
}

TEST(QnEmail, TrimInput)
{
    ASSERT_EQ(QnEmailAddress(kEmail), QnEmailAddress(" " + kEmail + " "));
}

TEST(QnEmail, ParseSimple)
{
    QnEmailAddress addr(kEmail);
    ASSERT_EQ(kEmail, addr.value());
    ASSERT_EQ(QString(), addr.fullName());
    ASSERT_EQ(kUser, addr.user());
    ASSERT_EQ(kDomain, addr.domain());
}

TEST(QnEmail, ParseName)
{
    QnEmailAddress addr(kEmailWithName);
    ASSERT_EQ(kEmail, addr.value());
    ASSERT_EQ(kName, addr.fullName());
    ASSERT_EQ(kUser, addr.user());
    ASSERT_EQ(kDomain, addr.domain());
}

TEST(QnEmail, ParseSuffix)
{
    QnEmailAddress addr(kEmailWithSuffix);
    ASSERT_EQ(kEmailWithSuffix, addr.value());
    ASSERT_EQ(QString(), addr.fullName());
    ASSERT_EQ(kUser, addr.user());
    ASSERT_EQ(kDomain, addr.domain());
}

TEST(QnEmail, ParseNameAndSuffix)
{
    QnEmailAddress addr(kEmailWithNameAndSuffix);
    ASSERT_EQ(kEmailWithSuffix, addr.value());
    ASSERT_EQ(kName, addr.fullName());
    ASSERT_EQ(kUser, addr.user());
    ASSERT_EQ(kDomain, addr.domain());
}

TEST(QnEmail, doubleDotIsInvalid)
{
    ASSERT_FALSE(isValidAddress("user@domain..net"));
}

} // namespace test
} // namespace nx::email