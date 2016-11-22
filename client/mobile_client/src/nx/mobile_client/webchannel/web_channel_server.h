#pragma once

#include <QtCore/QObject>
#include <QtWebChannel/QWebChannel>

class QWebSocketServer;

namespace nx {
namespace mobile_client {
namespace webchannel {

class WebChannelServerPrivate;
class WebChannelServer: public QWebChannel
{
    Q_OBJECT

public:
    WebChannelServer(QObject* parent = nullptr);

    bool isValid() const;

private:
    QWebSocketServer* m_server = nullptr;
};

} // namespace webchannel
} // namespace mobile_client
} // namespace nx
