#include "http_utils.h"

#include <stdexcept>

#include <QtCore/QString>

namespace nx::vms_server_plugins::analytics::vivotek {

using namespace nx::network::http;

void checkResponse(const AsyncClient& client)
{
    if (client.failed())
        throw std::system_error(
            client.lastSysErrorCode(), std::system_category(),
            "HTTP transport connection failed: " + client.url().toStdString());

    const auto& statusLine = client.response()->statusLine;
    if (statusLine.statusCode < 200 || 299 < statusLine.statusCode)
        throw std::runtime_error(
            QString("HTTP %1 %2 failed: %3")
                .arg(QString::fromUtf8(client.request().requestLine.method))
                .arg(client.url().toString())
                .arg(QString::fromUtf8(statusLine.reasonPhrase))
                .toStdString());
}

} // namespace nx::vms_server_plugins::analytics::vivotek
