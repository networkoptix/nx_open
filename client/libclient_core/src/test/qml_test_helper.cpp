#include "qml_test_helper.h"

#include <QtCore/QEventLoop>
#include <QtCore/QCoreApplication>
#include <QtCore/QtMath>

QnQmlTestHelper::QnQmlTestHelper(QObject* parent):
    QObject(parent)
{
}

int QnQmlTestHelper::random(int min, int max)
{
    return min + qrand() % (max - min);
}

QObject* QnQmlTestHelper::findChildObject(QObject* parent, const QString& childName)
{
    if (!parent)
        return nullptr;

    return parent->findChild<QObject*>(childName);
}
