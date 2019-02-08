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
    ASSERT_FALSE(isValidAddress("nhartleb@networkoptix..com"));
}

} // namespace test
} // namespace nx::email