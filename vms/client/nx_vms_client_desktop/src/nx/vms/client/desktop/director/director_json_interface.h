// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QJsonDocument>
#include <QtCore/QObject>

namespace nx::vms::client::desktop {

class Director;

/**
 * DirectorJsonInterface provides access to Director via JSON-encoded commands.
 * Director singleton must exist at the point when process() is called.
 * This class is currently stateless and several copies of it may be created as needed.
 */
class DirectorJsonInterface: public QObject
{
    Q_OBJECT
public:
    explicit DirectorJsonInterface(Director* director, QObject* parent);

    QJsonDocument process(const QJsonDocument& jsonRequest);

    enum ErrorCodes {Ok, BadCommand, BadParameters};

private:
    Director* m_director;
};

} // namespace nx::vms::client::desktop
