#pragma once

#include <QtCore/QObject>
#include <QtCore/QJsonDocument>

namespace nx::vmx::client::desktop {

/**
 * DirectorJsonInterface provides access to Director via JSON-encoded commands.
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
