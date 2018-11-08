#include <gtest/gtest.h>

#include <utils/email/email.h>

namespace {

static const QString kUser("cloud1");
static const QString kDomain("gdm.su");
static const QString kSuffix("suffix");
static const QString kName("S. Ivanov");

static const QString kEmail("cloud1@gdm.su");
static const QString kEmailWithName("S. Ivanov <cloud1@gdm.su>");
static const QString kEmailWithSuffix("cloud1+suffix@gdm.su");
static const QString kEmailWithNameAndSuffix("S. Ivanov <cloud1+suffix@gdm.su>");

}

TEST(QnEmail, EmptyValue)
{
    EXPECT_FALSE(QnEmailAddress("").isValid());
}

TEST(QnEmail, ValidAddr)
{
    EXPECT_TRUE(QnEmailAddress(kEmail).isValid());
}

TEST(QnEmail, ValidFullName)
{
    EXPECT_TRUE(QnEmailAddress(kEmailWithName).isValid());
}

TEST(QnEmail, ValidSuffix)
{
    EXPECT_TRUE(QnEmailAddress(kEmailWithSuffix).isValid());
}

TEST(QnEmail, ValidNameAndSuffix)
{
    EXPECT_TRUE(QnEmailAddress(kEmailWithNameAndSuffix).isValid());
}

TEST(QnEmail, TrimInput)
{
    EXPECT_EQ(QnEmailAddress(kEmail), QnEmailAddress(" " + kEmail + " "));
}

TEST(QnEmail, ParseSimple)
{
    QnEmailAddress addr(kEmail);
    EXPECT_EQ(kEmail, addr.value());
    EXPECT_EQ(QString(), addr.fullName());
    EXPECT_EQ(kUser, addr.user());
    EXPECT_EQ(kDomain, addr.domain());
}

TEST(QnEmail, ParseName)
{
    QnEmailAddress addr(kEmailWithName);
    EXPECT_EQ(kEmail, addr.value());
    EXPECT_EQ(kName, addr.fullName());
    EXPECT_EQ(kUser, addr.user());
    EXPECT_EQ(kDomain, addr.domain());
}

TEST(QnEmail, ParseSuffix)
{
    QnEmailAddress addr(kEmailWithSuffix);
    EXPECT_EQ(kEmailWithSuffix, addr.value());
    EXPECT_EQ(QString(), addr.fullName());
    EXPECT_EQ(kUser, addr.user());
    EXPECT_EQ(kDomain, addr.domain());
}

TEST(QnEmail, ParseNameAndSuffix)
{
    QnEmailAddress addr(kEmailWithNameAndSuffix);
    EXPECT_EQ(kEmailWithSuffix, addr.value());
    EXPECT_EQ(kName, addr.fullName());
    EXPECT_EQ(kUser, addr.user());
    EXPECT_EQ(kDomain, addr.domain());
}
