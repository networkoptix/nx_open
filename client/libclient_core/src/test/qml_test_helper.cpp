#include "qml_test_helper.h"

#include <QtCore/QEventLoop>
#include <QtCore/QCoreApplication>
#include <QtCore/QtMath>

#include <nx/utils/random.h>

QnQmlTestHelper::QnQmlTestHelper(QObject* parent):
    QObject(parent)
{
}

int QnQmlTestHelper::random(int min, int max)
{
    return nx::utils::random::number(min, max - 1);
}

QObject* QnQmlTestHelper::findChildObject(QObject* parent, const QString& childName)
{
    if (!parent)
        return nullptr;

    return parent->findChild<QObject*>(childName);
}
