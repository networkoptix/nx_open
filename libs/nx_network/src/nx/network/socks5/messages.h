// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/utils/buffer.h>

/**
 * Partial implementation of SOCKS5 protocol messages:
 *   SOCKS Protocol Version 5 - https://tools.ietf.org/html/rfc1928
 *   Username/Password Authentication for SOCKS V5 - https://tools.ietf.org/html/rfc1929
 */
namespace nx::network::socks5 {

inline constexpr uint8_t kSuccess = 0x00;
inline constexpr uint8_t kGeneralFailure = 0x01;
inline constexpr uint8_t kConnectionNotAllowed = 0x02;
inline constexpr uint8_t kNetworkUnreachable = 0x03;
inline constexpr uint8_t kHostUnreachable = 0x04;
inline constexpr uint8_t kConnectionRefused = 0x05;
inline constexpr uint8_t kTtlExpired = 0x06;
inline constexpr uint8_t kCommandNotSupported = 0x07;
inline constexpr uint8_t kAddressTypeNotSupported = 0x08;

inline constexpr uint8_t kMethodNoAuth = 0x00;
inline constexpr uint8_t kMethodUserPass = 0x02;
inline constexpr uint8_t kMethodUnacceptable = 0xFF;

inline constexpr uint8_t kCommandTcpConnection = 0x01;

/** Interface class for parsing and serializing protocol messages. */
struct Message
{
    enum class ParseStatus
    {
        Error,
        NeedMoreData,
        Complete
    };

    struct ParseResult
    {
        ParseStatus status = ParseStatus::Error;
        size_t size = 0;

        ParseResult(ParseStatus status, size_t size = 0): status(status), size(size) {}
    };

    virtual ParseResult parse(const nx::Buffer& buffer) = 0;
    virtual nx::Buffer toBuffer() const = 0;
};

/**
 * The client connects to the server, and sends a version
 * identifier/method selection message:
 *
 *    +----+----------+----------+
 *    |VER | NMETHODS | METHODS  |
 *    +----+----------+----------+
 *    | 1  |    1     | 1 to 255 |
 *    +----+----------+----------+
 */
struct NX_NETWORK_API GreetRequest: public Message
{
    std::vector<uint8_t> methods;

    GreetRequest(const std::vector<uint8_t>& methods = {}): methods(methods) {}

    virtual ParseResult parse(const nx::Buffer& buffer) override;
    virtual nx::Buffer toBuffer() const override;
};

/**
 * The server selects from one of the methods given in METHODS, and
 * sends a METHOD selection message:
 *
 *    +----+--------+
 *    |VER | METHOD |
 *    +----+--------+
 *    | 1  |   1    |
 *    +----+--------+
 */
struct NX_NETWORK_API GreetResponse: public Message
{
    uint8_t method = kMethodNoAuth;

    GreetResponse(uint8_t method = kMethodNoAuth): method(method) {}

    virtual ParseResult parse(const nx::Buffer& buffer) override;
    virtual nx::Buffer toBuffer() const override;
};

/**
 * The Username/Password subnegotiation begins with the client producing a
 * Username/Password request:
 *
 *    +----+------+----------+------+----------+
 *    |VER | ULEN |  UNAME   | PLEN |  PASSWD  |
 *    +----+------+----------+------+----------+
 *    | 1  |  1   | 1 to 255 |  1   | 1 to 255 |
 *    +----+------+----------+------+----------+
 */
struct NX_NETWORK_API AuthRequest: public Message
{
    std::string username;
    std::string password;

    AuthRequest() = default;

    AuthRequest(const std::string& username, const std::string& password):
        username(username),
        password(password)
    {}

    virtual ParseResult parse(const nx::Buffer& buffer) override;
    virtual nx::Buffer toBuffer() const override;
};

/**
 * The server verifies the supplied UNAME and PASSWD, and sends the
 * following response:
 *
 *    +----+--------+
 *    |VER | STATUS |
 *    +----+--------+
 *    | 1  |   1    |
 *    +----+--------+
 */
struct NX_NETWORK_API AuthResponse: public Message
{
    uint8_t status = kMethodUnacceptable;

    AuthResponse(uint8_t status = kMethodUnacceptable): status(status) {}

    virtual ParseResult parse(const nx::Buffer& buffer) override;
    virtual nx::Buffer toBuffer() const override;
};

/**
 * The SOCKS request is formed as follows:
 *
 *    +----+-----+-------+------+----------+----------+
 *    |VER | CMD |  RSV  | ATYP | DST.ADDR | DST.PORT |
 *    +----+-----+-------+------+----------+----------+
 *    | 1  |  1  | X'00' |  1   | Variable |    2     |
 *    +----+-----+-------+------+----------+----------+
 */
struct NX_NETWORK_API ConnectRequest: public Message
{
    uint8_t command = kCommandTcpConnection;
    std::string host;
    uint16_t port = 0;

    ConnectRequest() = default;

    ConnectRequest(const std::string& host, uint16_t port): host(host), port(port) {}

    virtual ParseResult parse(const nx::Buffer& buffer) override;
    virtual nx::Buffer toBuffer() const override;
};

/**
 * The server evaluates the connect request, and returns a reply formed as follows:
 *
 *    +----+-----+-------+------+----------+----------+
 *    |VER | REP |  RSV  | ATYP | BND.ADDR | BND.PORT |
 *    +----+-----+-------+------+----------+----------+
 *    | 1  |  1  | X'00' |  1   | Variable |    2     |
 *    +----+-----+-------+------+----------+----------+
 */
struct NX_NETWORK_API ConnectResponse: public Message
{
    uint8_t status = kGeneralFailure;
    std::string host;
    uint16_t port = 0;

    ConnectResponse() = default;

    ConnectResponse(uint8_t status, const std::string& host, uint16_t port):
        status(status),
        host(host),
        port(port)
    {}

    virtual ParseResult parse(const nx::Buffer& buffer) override;
    virtual nx::Buffer toBuffer() const override;
};

} // namespace nx::network::socks5
