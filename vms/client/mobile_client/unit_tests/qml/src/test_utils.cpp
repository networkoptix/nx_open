// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "test_utils.h"

#include <QtQml/QQmlContext>
#include <QtQml/QQmlEngine>
#include <QtQml/qqml.h>

namespace nx::vms::client::mobile::test {

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

void TestUtils::registerQmlType()
{
    qmlRegisterSingletonType<TestUtils>("Nx.Test",
        1,
        0,
        "TestUtils",
        [](QQmlEngine*, QJSEngine*) -> QObject* { return new TestUtils(); });
}

} // namespace nx::vms::client::mobile::test
