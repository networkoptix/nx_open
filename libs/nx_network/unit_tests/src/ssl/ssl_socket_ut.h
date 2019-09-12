#pragma once

#include <nx/network/ssl/ssl_stream_server_socket.h>
#include <nx/network/ssl/ssl_stream_socket.h>
#include <nx/network/system_socket.h>

namespace nx::network::ssl::test {

class EnforcedSslOverTcpServerSocket:
    public ssl::StreamServerSocket
{
    using base_type = ssl::StreamServerSocket;

public:
    EnforcedSslOverTcpServerSocket(int ipVersion = AF_INET):
        base_type(
            std::make_unique<TCPServerSocket>(ipVersion),
            ssl::EncryptionUse::always)
    {
    }
};

class AutoDetectedSslOverTcpServerSocket:
    public ssl::StreamServerSocket
{
    using base_type = ssl::StreamServerSocket;

public:
    AutoDetectedSslOverTcpServerSocket(int ipVersion = AF_INET):
        base_type(
            std::make_unique<TCPServerSocket>(ipVersion),
            ssl::EncryptionUse::autoDetectByReceivedData)
    {
    }
};

class SslOverTcpStreamSocket:
    public ssl::StreamSocket
{
    using base_type = ssl::StreamSocket;

public:
    SslOverTcpStreamSocket(int ipVersion = AF_INET):
        base_type(std::make_unique<TCPSocket>(ipVersion), false)
    {
    }
};

struct SslSocketBothEndsEncryptedTypeSet
{
    using ClientSocket = SslOverTcpStreamSocket;
    using ServerSocket = EnforcedSslOverTcpServerSocket;
};

struct SslSocketBothEndsEncryptedAutoDetectingServerTypeSet
{
    using ClientSocket = SslOverTcpStreamSocket;
    using ServerSocket = AutoDetectedSslOverTcpServerSocket;
};

struct SslSocketClientNotEncryptedTypeSet
{
    using ClientSocket = TCPSocket;
    using ServerSocket = AutoDetectedSslOverTcpServerSocket;
};

} // namespace nx::network::ssl::test
