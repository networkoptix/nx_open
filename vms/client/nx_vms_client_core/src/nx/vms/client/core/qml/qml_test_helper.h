// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

namespace nx::vms::client::core {

class QmlTestHelper : public QObject
{
    Q_OBJECT

public:
    QmlTestHelper(QObject* parent = nullptr);

public slots:
    int random(int min, int max);
    QObject* findChildObject(QObject* parent, const QString& childName);
};

} // namespace nx::vms::client::core
