// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

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
