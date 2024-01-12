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

    /**
    * @addtogroup vms-log
    * This section contains specification for the logging support inside web pages.
    *
    * Use this object to log messages using the standard VMS logging system.
    *
    * Example:
    *
    *     vms.log.warning("Something strange happened here!")
    *
    * @{
    */

    /** Logs the message with the ERROR log level. */
    Q_INVOKABLE void error(const QString& message) const;

    /** Logs the message with the WARNING log level. */
    Q_INVOKABLE void warning(const QString& message) const;

    /** Logs the message with the INFO log level. */
    Q_INVOKABLE void info(const QString& message) const;

    /** Logs the message with the DEBUG log level. */
    Q_INVOKABLE void debug(const QString& message) const;

    /** Logs the message with the VERBOSE log level. */
    Q_INVOKABLE void verbose(const QString& message) const;

    /** @} */ // group vms-log
};

} // namespace nx::vms::client::desktop::jsapi
