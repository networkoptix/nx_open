// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

namespace nx::vms::client::desktop::jsapi {

class Logger: public QObject
{
    Q_OBJECT
    using base_type = QObject;

public:
    Logger(QObject* parent);

    Q_INVOKABLE void error(const QString& message) const;
    Q_INVOKABLE void warning(const QString& message) const;
    Q_INVOKABLE void info(const QString& message) const;
    Q_INVOKABLE void debug(const QString& message) const;
    Q_INVOKABLE void verbose(const QString& message) const;
};

} // namespace nx::vms::client::desktop::jsapi
