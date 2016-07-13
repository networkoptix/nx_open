#include "qml_test_helper.h"

#include <QtCore/QEventLoop>
#include <QtCore/QCoreApplication>
#include <QtCore/QtMath>

#include <utils/common/util.h>

QnQmlTestHelper::QnQmlTestHelper(QObject* parent):
    QObject(parent)
{
}

int QnQmlTestHelper::random(int min, int max)
{
    return ::random(min, max);
}

QObject* QnQmlTestHelper::findChildObject(QObject* parent, const QString& childName)
{
    if (!parent)
        return nullptr;

    return parent->findChild<QObject*>(childName);
}
