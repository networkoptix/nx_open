#include <gtest/gtest.h>

#include <QtCore/QString>

#include <nx/utils/member_detector.h>

namespace {

struct HasStringField
{
    QString stringField;
};

struct HasOtherStringField
{
    QString otherStringField;
};

struct HasStructField
{
    HasStringField structField;
};

struct HasTemplatedField
{
    std::set<QString> templatedField;
};

struct HasOtherTemplatedField
{
    std::set<QString> otherTemplatedField;
};

struct HasSomeMethod
{
    int method(double& /*someArg*/) { return 0; };
};

NX_UTILS_DECLARE_FIELD_DETECTOR(hasStringField, stringField, QString);
NX_UTILS_DECLARE_FIELD_DETECTOR(hasStructField, structField, HasStringField);
NX_UTILS_DECLARE_METHOD_DETECTOR(hasSomeMethod, method, int(double&));
NX_UTILS_DECLARE_FIELD_DETECTOR(hasTemplatedField, templatedField, std::set<QString>);
NX_UTILS_DECLARE_FIELD_DETECTOR_SIMPLE(hasStringFieldSimple, stringField);

} // namespace

TEST(MemberDetector, detect)
{
    ASSERT_TRUE(hasStringField<HasStringField>::value);
    ASSERT_FALSE(hasStringField<HasOtherStringField>::value);
    ASSERT_FALSE(hasStringField<HasStructField>::value);
    ASSERT_FALSE(hasStringField<HasSomeMethod>::value);
    ASSERT_FALSE(hasStringField<HasTemplatedField>::value);
    ASSERT_FALSE(hasStringField<HasOtherTemplatedField>::value);

    ASSERT_FALSE(hasStructField<HasStringField>::value);
    ASSERT_TRUE(hasStructField<HasStructField>::value);
    ASSERT_FALSE(hasStructField<HasSomeMethod>::value);
    ASSERT_FALSE(hasStructField<HasTemplatedField>::value);
    ASSERT_FALSE(hasStructField<HasOtherTemplatedField>::value);

    ASSERT_FALSE(hasSomeMethod<HasStringField>::value);
    ASSERT_FALSE(hasSomeMethod<HasStructField>::value);
    ASSERT_TRUE(hasSomeMethod<HasSomeMethod>::value);
    ASSERT_FALSE(hasSomeMethod<HasTemplatedField>::value);
    ASSERT_FALSE(hasSomeMethod<HasOtherTemplatedField>::value);

    ASSERT_FALSE(hasTemplatedField<HasStringField>::value);
    ASSERT_FALSE(hasTemplatedField<HasStructField>::value);
    ASSERT_FALSE(hasTemplatedField<HasSomeMethod>::value);
    ASSERT_TRUE(hasTemplatedField<HasTemplatedField>::value);
    ASSERT_FALSE(hasTemplatedField<HasOtherTemplatedField>::value);

    ASSERT_TRUE(hasStringFieldSimple<HasStringField>::value);
    ASSERT_FALSE(hasStringFieldSimple<HasOtherStringField>::value);
    ASSERT_FALSE(hasStringFieldSimple<HasStructField>::value);
    ASSERT_FALSE(hasStringFieldSimple<HasSomeMethod>::value);
    ASSERT_FALSE(hasStringFieldSimple<HasTemplatedField>::value);
    ASSERT_FALSE(hasStringFieldSimple<HasOtherTemplatedField>::value);
}
