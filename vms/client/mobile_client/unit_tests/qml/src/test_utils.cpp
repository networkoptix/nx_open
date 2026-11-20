// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "test_utils.h"

#include <algorithm>

#include <QtCore/QMetaMethod>
#include <QtCore/QMetaProperty>
#include <QtCore/QSet>
#include <QtQml/QQmlContext>
#include <QtQml/QQmlEngine>
#include <QtQml/qqml.h>

#include <nx/utils/log/format.h>
#include <nx/utils/qobject.h>

namespace nx::vms::client::mobile::test {

namespace {

const QByteArray kResetFunction = "reset";
const QByteArray kCallSignalSuffix = "Called";

bool isObjectType(QMetaType type)
{
    return type.flags().testFlag(QMetaType::PointerToQObject);
}

bool isSignal(const QMetaMethod& method)
{
    return method.methodType() == QMetaMethod::Signal;
}

QString memberPath(const QString& path, const QByteArray& name)
{
    if (path.isEmpty())
        return QString::fromLatin1(name);

    return nx::format("%1.%2", path, name);
}

bool isSameType(const QMetaProperty& mockProperty, const QMetaProperty& property)
{
    // Members of the objects are compared separately.
    if (isObjectType(property.metaType()))
        return isObjectType(mockProperty.metaType());

    // QML has no enumeration properties, they are int there.
    if (property.isEnumType())
        return mockProperty.metaType() == QMetaType::fromType<int>();

    return mockProperty.metaType() == property.metaType();
}

/** Same kind, name and number of parameters. */
bool hasSameMethod(const QMetaObject* type, const QMetaMethod& method)
{
    const auto candidates = nx::utils::findMethods(type, method.name());
    return std::any_of(candidates.begin(),
        candidates.end(),
        [&method](const QMetaMethod& candidate)
        {
            return isSignal(candidate) == isSignal(method)
                && candidate.parameterCount() == method.parameterCount()
                && candidate.access() != QMetaMethod::Private;
        });
}

/** `<function>Called` signals of the mock functions. */
QSet<int> callSignals(const QObject* mock)
{
    QSet<int> result;
    for (const auto& method: nx::utils::methods(mock))
    {
        if (isSignal(method))
            continue;

        if (const auto signal = nx::utils::findSignal(mock, method.name() + kCallSignalSuffix))
            result.insert(signal->methodIndex());
    }

    return result;
}

void collectMismatches(
    const QObject* mock, const QMetaObject* type, const QString& path, QStringList* result);

void collectPropertyMismatches(
    const QObject* mock, const QMetaObject* type, const QString& path, QStringList* result)
{
    const auto mockType = mock->metaObject();

    // Sorted for a stable report.
    auto names = nx::utils::propertyNames(mock).values();
    std::sort(names.begin(), names.end());

    for (const auto& name: names)
    {
        const auto mockProperty = mockType->property(mockType->indexOfProperty(name));
        if (!mockProperty.isValid())
            continue; //< Dynamic, invisible to QML.

        const QString member = memberPath(path, name);
        const int index = type->indexOfProperty(name);
        if (index < 0)
        {
            *result << nx::format("%1: no such property in %2", member, type->className());
            continue;
        }

        const auto property = type->property(index);
        if (!isSameType(mockProperty, property))
        {
            *result << nx::format("%1: the type is %2, while it is %3 in %4",
                member,
                mockProperty.typeName(),
                property.typeName(),
                type->className());
            continue;
        }

        const auto nestedType = property.metaType().metaObject();
        if (!nestedType)
            continue;

        if (const auto nestedMock = mockProperty.read(mock).value<QObject*>())
            collectMismatches(nestedMock, nestedType, member, result);
    }
}

void collectMethodMismatches(
    const QObject* mock, const QMetaObject* type, const QString& path, QStringList* result)
{
    const bool isRoot = path.isEmpty();
    const auto mockOnlySignals = callSignals(mock);

    for (const auto& method: nx::utils::methods(mock))
    {
        if (nx::utils::isNotifySignal(mock->metaObject(), method)
            || mockOnlySignals.contains(method.methodIndex())
            || (isRoot && !isSignal(method) && method.name() == kResetFunction))
        {
            continue;
        }

        if (!hasSameMethod(type, method))
        {
            *result << nx::format("%1: no such %2 with %3 parameter(s) in %4",
                memberPath(path, method.name()),
                isSignal(method) ? "signal" : "function",
                method.parameterCount(),
                type->className());
        }
    }
}

void collectMismatches(
    const QObject* mock, const QMetaObject* type, const QString& path, QStringList* result)
{
    collectPropertyMismatches(mock, type, path, result);
    collectMethodMismatches(mock, type, path, result);
}

} // namespace

QObject* TestUtils::findById(QObject* root, const QString& id) const
{
    const auto context = qmlContext(root);
    if (!context)
        return nullptr;

    if (context->contextObject() == root)
        return context->findObjectRecursively(id);

    // An object declared next to others shares their context, its own one is among the children.
    for (const auto child: context->childContexts())
    {
        if (child->contextObject() == root)
            return child->findObjectRecursively(id);
    }

    return nullptr;
}

QStringList TestUtils::mockMismatches(QObject* mock, const QString& typeName) const
{
    if (!mock)
        return {"The mock is null"};

    const auto type = QMetaType::fromName(typeName.toLatin1() + '*').metaObject();
    if (!type)
        return {nx::format("%1 is not a class known to the meta type system", typeName)};

    QStringList result;
    collectMismatches(mock, type, /*path*/ {}, &result);
    return result;
}

void TestUtils::registerQmlType()
{
    qmlRegisterSingletonType<TestUtils>("Nx.Test",
        1,
        0,
        "TestUtils",
        [](QQmlEngine*, QJSEngine*) -> QObject* { return new TestUtils(); });
}

} // namespace nx::vms::client::mobile::test
