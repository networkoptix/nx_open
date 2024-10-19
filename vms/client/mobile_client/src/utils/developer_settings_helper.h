// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

namespace nx {
namespace client {
namespace mobile {
namespace utils {

class DeveloperSettingsHelper: public QObject
{
    Q_OBJECT

    Q_PROPERTY(QString logLevel READ logLevel WRITE setLogLevel NOTIFY logLevelChanged)

public:
    explicit DeveloperSettingsHelper(QObject* parent = nullptr);

    QString logLevel() const;
    void setLogLevel(QString logLevel);

signals:
    void logLevelChanged();
};

} // namespace utils
} // namespace mobile
} // namespace client
} // namespace nx
