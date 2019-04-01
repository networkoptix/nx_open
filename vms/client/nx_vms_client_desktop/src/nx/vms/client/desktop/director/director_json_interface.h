#pragma once

#include <QtCore/QObject>
#include <QtCore/QJsonDocument>

namespace nx::vmx::client::desktop {

/**
 * DirectorJsonInterface provides access to Director via JSON-encoded commands.
 * Director singleton must exist at the point when process() is called.
 * This class is currently stateless and several copies of it may be created as needed.
 */
class DirectorJsonInterface: public QObject
{
    Q_OBJECT
public:
    explicit DirectorJsonInterface(QObject* parent);

    QJsonDocument process(const QJsonDocument& jsonRequest);

    enum ErrorCodes {Ok, BadCommand, BadParameters};
};

} // namespace nx::vmx::client::desktop
