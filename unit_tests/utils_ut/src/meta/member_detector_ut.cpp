#include <gtest/gtest.h>

#include <nx/utils/meta/member_detector.h>

namespace {

struct HasStringField
{
    QString stringField;
};

struct HasStructField
{
    HasStringField structField;
};

struct HasSomeMethod
{
    int method(double& /*someArg*/) {};
};

DECLARE_FIELD_DETECTOR(hasStringField, stringField, QString);
DECLARE_FIELD_DETECTOR(hasStructField, structField, HasStringField);
DECLARE_METHOD_DETECTOR(hasSomeMethod, method, int(double&));

} // namespace

TEST(MemberDetector, detect)
{
    ASSERT_TRUE(hasStringField<HasStringField>::value);
    ASSERT_FALSE(hasStringField<HasStructField>::value);
    ASSERT_FALSE(hasStringField<HasSomeMethod>::value);

    ASSERT_FALSE(hasStructField<HasStringField>::value);
    ASSERT_TRUE(hasStructField<HasStructField>::value);
    ASSERT_FALSE(hasStructField<HasSomeMethod>::value);

    ASSERT_FALSE(hasSomeMethod<HasStringField>::value);
    ASSERT_FALSE(hasSomeMethod<HasStructField>::value);
    ASSERT_TRUE(hasSomeMethod<HasSomeMethod>::value);
}
