// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <memory>

#include <QtQml/QQmlComponent>
#include <QtQml/QQmlEngine>

#include <nx/reflect/enum_instrument.h>
#include <nx/vms/client/core/qml/enums_as_singletons.h>

namespace nx::vms::client::core::test {

NX_REFLECTION_ENUM(NxEnum,
    value1 = 1,
    Value10 = 10,
    _value100 = 100
);

NX_REFLECTION_ENUM_CLASS(NxEnumClass,
    value1 = 1,
    Value10 = 10,
    _value100 = 100
);

// Current implementation allows only registration of enums at a module scope, so registering
// enums in classes (thus moving them up to the module scope) is not recommended, but it works
// and is being tested here.

struct NxEnums
{
    NX_REFLECTION_ENUM_IN_CLASS(NxEnumInClass,
        value1 = 1,
        Value10 = 10,
        _value100 = 100
    );

    NX_REFLECTION_ENUM_CLASS_IN_CLASS(NxEnumClassInClass,
        value1 = 1,
        Value10 = 10,
        _value100 = 100
    );
};

struct QtEnums
{
    Q_GADGET

public:
    enum QtEnum
    {
        value1 = 1,
        Value10 = 10,
        _value100 = 100
    };
    Q_ENUM(QtEnum)

    enum class QtEnumClass
    {
        value1 = 1,
        Value10 = 10,
        _value100 = 100
    };
    Q_ENUM(QtEnumClass)
};

namespace enums_ns {

Q_NAMESPACE

enum QtEnumNs
{
    value1 = 1,
    Value10 = 10,
    _value100 = 100
};
Q_ENUM_NS(QtEnumNs)

enum class QtEnumClassNs
{
    value1 = 1,
    Value10 = 10,
    _value100 = 100
};
Q_ENUM_NS(QtEnumClassNs)

} // namespace enums_ns

class EnumsAsSingletonsTest:
    public ::testing::Test,
    public ::testing::WithParamInterface<QString>
{
public:
    static void SetUpTestSuite()
    {
        EXPECT_GE(nxRegisterQmlEnumType<NxEnum>("nx.vms.client.core.test", 1, 0,
            "NxEnum"), 0);

        EXPECT_GE(nxRegisterQmlEnumType<NxEnumClass>("nx.vms.client.core.test", 1, 0,
            "NxEnumClass"), 0);

        EXPECT_GE(nxRegisterQmlEnumType<NxEnums::NxEnumInClass>("nx.vms.client.core.test", 1, 0,
            "NxEnumInClass"), 0);

        EXPECT_GE(nxRegisterQmlEnumType<NxEnums::NxEnumClassInClass>("nx.vms.client.core.test",
            1, 0, "NxEnumClassInClass"), 0);

        EXPECT_GE(nxRegisterQmlEnumType<QtEnums::QtEnum>("nx.vms.client.core.test", 1, 0,
            "QtEnum"), 0);

        EXPECT_GE(nxRegisterQmlEnumType<QtEnums::QtEnumClass>("nx.vms.client.core.test", 1, 0,
            "QtEnumClass"), 0);

        EXPECT_GE(nxRegisterQmlEnumType<enums_ns::QtEnumNs>("nx.vms.client.core.test", 1, 0,
            "QtEnumNs"), 0);

        EXPECT_GE(nxRegisterQmlEnumType<enums_ns::QtEnumClassNs>("nx.vms.client.core.test", 1, 0,
            "QtEnumClassNs"), 0);
    }

protected:
    virtual void SetUp() override
    {
        engine.reset(new QQmlEngine());
    }

    virtual void TearDown() override
    {
        engine.reset();
    }

    std::unique_ptr<QQmlEngine> engine;
};

TEST_P(EnumsAsSingletonsTest, accessFromQml)
{
    static const QString componentTemplate = R"(
            import QtQml
            import nx.vms.client.core.test

            QtObject
            {
                readonly property int v1: %1.value1
                readonly property int v10: %1.Value10
                readonly property int v100: %1._value100
            }
        )";

    const auto componentText = componentTemplate.arg(GetParam());

    QQmlComponent component(engine.get());
    component.setData(componentText.toLatin1(), QUrl{});

    ASSERT_TRUE(component.isReady());
    const std::unique_ptr<QObject> object{component.create()};

    EXPECT_EQ(object->property("v1"), 1);
    EXPECT_EQ(object->property("v10"), 10);
    EXPECT_EQ(object->property("v100"), 100);
}

INSTANTIATE_TEST_SUITE_P(,
    EnumsAsSingletonsTest,
    ::testing::Values(
        "NxEnum",
        "NxEnumClass",
        "NxEnumInClass",
        "NxEnumClassInClass",
        "QtEnum",
        "QtEnumClass",
        "QtEnumNs",
        "QtEnumClassNs"));

} // namespace nx::vms::client::core::test

#include "enums_as_singletons_ut.moc"
