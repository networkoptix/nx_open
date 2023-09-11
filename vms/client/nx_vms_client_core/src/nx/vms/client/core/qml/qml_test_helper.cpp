// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "qml_test_helper.h"

#include <QtCore/QEventLoop>
#include <QtCore/QCoreApplication>
#include <QtCore/QtMath>

#include <nx/utils/random.h>

namespace nx::vms::client::core {

QmlTestHelper::QmlTestHelper(QObject* parent):
    QObject(parent)
{
}

int QmlTestHelper::random(int min, int max)
{
    return nx::utils::random::number(min, max - 1);
}

QObject* QmlTestHelper::findChildObject(QObject* parent, const QString& childName)
{
    if (!parent)
        return nullptr;

    return parent->findChild<QObject*>(childName);
}

} // namespace nx::vms::client::core
