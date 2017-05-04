#include "to_string.h"

QString toString(const char* s)
{
    return QString::fromUtf8(s);
}

QString toString(void* p)
{
    return QString::fromLatin1("0x%1").arg(reinterpret_cast<qulonglong>(p), 0, 16);
}

QString toString(const QByteArray& t)
{
    return QString::fromUtf8(t);
}

QString toString(const QUrl& url)
{
    return url.toString(QUrl::RemovePassword);
}

QString toString(const std::string& t)
{
    return QString::fromStdString(t);
}

QString toString(const std::chrono::hours& t)
{
    return QString(QLatin1String("%1h")).arg(t.count());
}

QString toString(const std::chrono::minutes& t)
{
    if (t.count() % 60 == 0)
        return toString(std::chrono::duration_cast<std::chrono::hours>(t));

    return QString(QLatin1String("%1m")).arg(t.count());
}

QString toString(const std::chrono::seconds& t)
{
    if (t.count() % 1000 == 0)
        return toString(std::chrono::duration_cast<std::chrono::minutes>(t));

    return QString(QLatin1String("%1s")).arg(t.count());
}

QString toString(const std::chrono::milliseconds& t)
{
    if (t.count() % 1000 == 0)
        return toString(std::chrono::duration_cast<std::chrono::seconds>(t));

    return QString(QLatin1String("%1ms")).arg(t.count());
}

QString toString(QAbstractSocket::SocketError error)
{
    switch (error)
    {
        case QAbstractSocket::ConnectionRefusedError:
            return QString(QLatin1String("Connection refused"));
        case QAbstractSocket::RemoteHostClosedError:
            return QString(QLatin1String("Remote host closed the connection"));
        case QAbstractSocket::HostNotFoundError:
            return QString(QLatin1String("Host was not found"));
        case QAbstractSocket::SocketAccessError:
            return QString(QLatin1String("Socket access error"));
        case QAbstractSocket::SocketResourceError:
            return QString(QLatin1String("Socket resource error"));
        case QAbstractSocket::SocketTimeoutError:
            return QString(QLatin1String("Socket operation timed out"));
        case QAbstractSocket::DatagramTooLargeError:
            return QString(QLatin1String("Datagram is to large"));
        case QAbstractSocket::NetworkError:
            return QString(QLatin1String("Network error"));
        case QAbstractSocket::AddressInUseError:
            return QString(QLatin1String("Address is already in use"));
        case QAbstractSocket::SocketAddressNotAvailableError:
            return QString(QLatin1String("Address does not belong to host"));
        case QAbstractSocket::UnsupportedSocketOperationError:
            return QString(QLatin1String("Requested operation is not supported"));
        case QAbstractSocket::ProxyAuthenticationRequiredError:
            return QString(QLatin1String("Proxy authentication required"));
        case QAbstractSocket::SslHandshakeFailedError:
            return QString(QLatin1String("SSL handshake failed"));
        case QAbstractSocket::UnfinishedSocketOperationError:
            return QString(QLatin1String("The last operation is not finished yet"));
        case QAbstractSocket::ProxyConnectionRefusedError:
            return QString(QLatin1String("Proxy connection refused"));
        case QAbstractSocket::ProxyConnectionClosedError:
            return QString(QLatin1String("Proxy connection closed"));
        case QAbstractSocket::ProxyConnectionTimeoutError:
            return QString(QLatin1String("Proxy connection timeout"));
        case QAbstractSocket::ProxyNotFoundError:
            return QString(QLatin1String("Proxy not found"));
        case QAbstractSocket::ProxyProtocolError:
            return QString(QLatin1String("Proxy protocol error"));
        case QAbstractSocket::OperationError:
            return QString(QLatin1String("Socket operation error"));
        case QAbstractSocket::SslInternalError:
            return QString(QLatin1String("Ssl internal error"));
        case QAbstractSocket::SslInvalidUserDataError:
            return QString(QLatin1String("Ssl invalid user data"));
        case QAbstractSocket::TemporaryError:
            return QString(QLatin1String("Temporary error"));
        case QAbstractSocket::UnknownSocketError:
            return QString(QLatin1String("Unknown error"));
        default:
            return QString(QLatin1String("Unknown error (not recognized by nx)"));
    }
}
