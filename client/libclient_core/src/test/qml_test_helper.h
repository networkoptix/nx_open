#pragma once

#include <QtCore/QObject>

class QnQmlTestHelper : public QObject
{
    Q_OBJECT

public:
    QnQmlTestHelper(QObject* parent = nullptr);

public slots:
    int random(int min, int max);
    QObject* findChildObject(QObject* parent, const QString& childName);
};
