// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <QtQml/QQmlEngine>
#include <QtQml/QQmlComponent>

#include <nx/vms/client/core/utils/qml_property.h>

namespace nx::vms::client::core {

class QmlPropertyTest: public ::testing::Test
{
protected:
    std::unique_ptr<QObject> load(const QString& path);

    QQmlEngine engine;
};

std::unique_ptr<QObject> QmlPropertyTest::load(const QString& path)
{
    QQmlComponent component(
        &engine, ":/unit_tests/qml_property/" + path, QQmlComponent::PreferSynchronous);
    return std::unique_ptr<QObject>(component.create());
}

TEST_F(QmlPropertyTest, trivialReadWrite)
{
    auto object = load("Trivial.qml");
    ASSERT_TRUE(object);

    const QmlProperty<int> a{object.get(), "a"};
    ASSERT_TRUE(a.isValid());
    ASSERT_TRUE(a.hasNotifySignal());

    a = 42;
    ASSERT_EQ(a, 42);
}

TEST_F(QmlPropertyTest, trivialNotify)
{
    auto object = load("Trivial.qml");
    ASSERT_TRUE(object);

    const QmlProperty<int> a{object.get(), "a"};

    int value = 0;
    a.connectNotifySignal([&value, a]() { value = a; });

    a = 42;
    ASSERT_EQ(value, 42);
}

TEST_F(QmlPropertyTest, trivialObjectHolder)
{
    QmlPropertyBase::ObjectHolder holder;

    const QmlProperty<int> a{&holder, "a"};

    ASSERT_FALSE(a.isValid());

    auto object = load("Trivial.qml");
    holder.setObject(object.get());

    ASSERT_TRUE(a.isValid());
    a = 42;
    ASSERT_EQ(a, 42);
}

TEST_F(QmlPropertyTest, notificationsWithObjectHolder)
{
    QmlPropertyBase::ObjectHolder holder;

    const QmlProperty<int> a{&holder, "a"};

    int value = 0;
    a.connectNotifySignal([&value, a]() { value = a; });

    auto object = load("Trivial.qml");
    holder.setObject(object.get());
    a = 42;
    ASSERT_EQ(value, 42);
}

TEST_F(QmlPropertyTest, notificationsWithObjectProperty)
{
    auto root = load("Nested.qml");

    QmlProperty<QObject*> nested{root.get(), "nested"};
    ASSERT_EQ(nested, nullptr);

    QmlProperty<int> a(&nested, "a");
    ASSERT_FALSE(a.isValid());

    int value = 0;
    a.connectNotifySignal([&value, a]() { value = a; });

    auto nestedObject = load("Trivial.qml");
    nested = nestedObject.get();

    ASSERT_TRUE(a.isValid());
    a = 42;
    ASSERT_EQ(value, 42);
}

TEST_F(QmlPropertyTest, valueIsAvailableImmediatelyAfterConstructor)
{
    auto object = load("Trivial.qml");
    QmlPropertyBase::ObjectHolder holder;
    holder.setObject(object.get());

    const QmlProperty<int> a{&holder, "a"};
    ASSERT_TRUE(a.isValid());
    ASSERT_NE(a, 0);
}

TEST_F(QmlPropertyTest, propertyAssignment)
{
    auto object = load("Trivial.qml");

    QmlProperty<int> a;
    QmlProperty<int> a1{object.get(), "a"};

    a = a1;

    int value = 0;
    a1.connectNotifySignal([&value, a1]() { value = a1; });

    a = 42;

    ASSERT_EQ(value, a);
}

} // namespace nx::vms::client::core
