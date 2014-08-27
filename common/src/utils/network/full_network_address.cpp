#include "full_network_address.h"

#include <QtCore/QUrl>

QnNetworkAddress::QnNetworkAddress() :
    m_port(0)
{
}

QnNetworkAddress::QnNetworkAddress(const QHostAddress &address, quint16 port) :
    m_host(address),
    m_port(port)
{
}

QnNetworkAddress::QnNetworkAddress(const QUrl &url) :
    m_host(url.host()),
    m_port(url.port())
{
}

bool QnNetworkAddress::isValid() const {
    return !m_host.isNull();
}

bool QnNetworkAddress::operator ==(const QnNetworkAddress &other) const {
    return m_host == other.m_host && m_port == other.m_port;
}

QHostAddress QnNetworkAddress::host() const {
    return m_host;
}

void QnNetworkAddress::setHost(const QHostAddress &address) {
    m_host = address;
}

quint16 QnNetworkAddress::port() const {
    return m_port;
}

void QnNetworkAddress::setPort(quint16 port) {
    m_port = port;
}

QUrl QnNetworkAddress::toUrl() const {
    QUrl url;
    url.setScheme(lit("http"));
    url.setHost(m_host.toString());
    url.setPort(m_port);
    return url;
}

uint qHash(const QnNetworkAddress &address, uint seed) {
    return qHash(address.host(), seed + address.port());
}
