// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

namespace nx::vms::client::core {

/** Singleton helper class to interact with files in QML. */
class FileIO: public QObject
{
    Q_OBJECT
    using base_type = QObject;

public:
    static void registerQmlType();

    Q_INVOKABLE QString readAll(const QString& fileName) const;
};

} // namespace nx::vms::client::core
