// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <nx/network/socks5/messages.h>

namespace nx::network::socks5::test {

using namespace nx::network::socks5;

using Data = std::vector<uint8_t>;

class GreetRequestInvalid: public ::testing::TestWithParam<Data> {};
class GreetRequestIncomplete: public ::testing::TestWithParam<Data> {};

class GreetResponseInvalid: public ::testing::TestWithParam<Data> {};
class GreetResponseIncomplete: public ::testing::TestWithParam<Data> {};

class AuthRequestInvalid: public ::testing::TestWithParam<Data> {};
class AuthRequestIncomplete: public ::testing::TestWithParam<Data> {};

class AuthResponseInvalid: public ::testing::TestWithParam<Data> {};
class AuthResponseIncomplete: public ::testing::TestWithParam<Data> {};

class ConnectRequestInvalid: public ::testing::TestWithParam<Data> {};
class ConnectRequestIncomplete: public ::testing::TestWithParam<Data> {};

class ConnectResponseInvalid: public ::testing::TestWithParam<Data> {};
class ConnectResponseIncomplete: public ::testing::TestWithParam<Data> {};

TEST(Socks5Messages, GreetRequest_parse_username_password_authorization_method)
{
    static constexpr uint8_t data[] = {
        0x05, 0x01, 0x02
    };

    const nx::Buffer buffer((char*) &data, sizeof(data));
    GreetRequest request;

    EXPECT_EQ(buffer.size(), request.parse(buffer).size);
    EXPECT_EQ(1, request.methods.size());
    EXPECT_EQ(0x02, request.methods[0]);
}

TEST_P(GreetRequestInvalid, parse)
{
    const auto data = GetParam();
    GreetRequest request;

    const auto parseResult = request.parse(nx::Buffer((char*) data.data(), data.size()));

    EXPECT_EQ(Message::ParseStatus::Error, parseResult.status);
    EXPECT_EQ(0, parseResult.size);
}

INSTANTIATE_TEST_SUITE_P(
    Socks5Messages,
    GreetRequestInvalid,
    ::testing::Values(
        Data{ 0x05, 0x00 },
        Data{ 0x01, 0x01 }
    ));

TEST_P(GreetRequestIncomplete, parse)
{
    const auto data = GetParam();
    GreetRequest request;

    const auto parseResult = request.parse(nx::Buffer((char*) data.data(), data.size()));

    EXPECT_EQ(Message::ParseStatus::NeedMoreData, parseResult.status);
    EXPECT_EQ(0, parseResult.size);
}

INSTANTIATE_TEST_SUITE_P(
    Socks5Messages,
    GreetRequestIncomplete,
    ::testing::Values(
        Data{ 0x05 },
        Data{ 0x05, 0x01 },
        Data{ 0x05, 0x03, 0x00 },
        Data{ 0x05, 0x03, 0x00, 0x02 },
        Data{ 0x05, 0x09, 0x00, 0x00, 0x00 }
    ));

TEST(Socks5Messages, GreetRequest_toBuffer)
{
    static constexpr uint8_t data[] = {
        0x05, 0x02, 0x02, 0x00
    };
    const nx::Buffer expected((char*) &data, sizeof(data));

    GreetRequest request;
    request.methods = { 0x02, 0x00 };

    EXPECT_EQ(expected, request.toBuffer());
}

TEST(Socks5Messages, GreetResponse_parse_method)
{
    static constexpr uint8_t data[] = {
        0x05, 0x02
    };

    const nx::Buffer buffer((char*) &data, sizeof(data));
    GreetResponse response;

    const auto parseResult = response.parse(buffer);

    EXPECT_EQ(Message::ParseStatus::Complete, parseResult.status);
    EXPECT_EQ(buffer.size(), parseResult.size);

    EXPECT_EQ(0x02, response.method);
}

TEST_P(GreetResponseInvalid, parse)
{
    const auto data = GetParam();
    GreetResponse response;
    EXPECT_EQ(
        Message::ParseStatus::Error,
        response.parse(nx::Buffer((char*) data.data(), data.size())).status);
}

INSTANTIATE_TEST_SUITE_P(
    Socks5Messages,
    GreetResponseInvalid,
    ::testing::Values(
        Data{ 0xFF, 0x01 },
        Data{ 0x01, 0x00 }
    ));

TEST_P(GreetResponseIncomplete, parse)
{
    const auto data = GetParam();
    GreetResponse response;
    EXPECT_EQ(
        Message::ParseStatus::NeedMoreData,
        response.parse(nx::Buffer((char*) data.data(), data.size())).status);
}

INSTANTIATE_TEST_SUITE_P(
    Socks5Messages,
    GreetResponseIncomplete,
    ::testing::Values(
        Data{ 0x05 }
    ));

TEST(Socks5Messages, GreetResponse_toBuffer)
{
    static constexpr uint8_t data[] = {
        0x05, 0x02
    };
    const nx::Buffer expected((char*) &data, sizeof(data));

    GreetResponse response(0x02);

    EXPECT_EQ(expected, response.toBuffer());
}

TEST(Socks5Messages, AuthRequest_parse_username_and_password)
{
    static constexpr uint8_t data[] = {
        0x01, 0x04, 'u', 's', 'e', 'r', 0x08, 'p', 'a', 's', 's', 'w', 'o', 'r', 'd'
    };

    const nx::Buffer buffer((char*) &data, sizeof(data));
    AuthRequest request;

    EXPECT_EQ(buffer.size(), request.parse(buffer).size);
    EXPECT_EQ("user", request.username);
    EXPECT_EQ("password", request.password);
}

TEST_P(AuthRequestInvalid, parse)
{
    const auto data = GetParam();
    AuthRequest request;
    EXPECT_EQ(
        Message::ParseStatus::Error,
        request.parse(nx::Buffer((char*) data.data(), data.size())).status);
}

INSTANTIATE_TEST_SUITE_P(
    Socks5Messages,
    AuthRequestInvalid,
    ::testing::Values(
        Data{ 0x05, 0x02, 0x01, 'u', 0x01, 'p' },
        Data{ 0x01, 0x00, 0x00 },
        Data{ 0x01, 0x00, 0x08, 'p', 'a', 's', 's', 'w', 'o', 'r', 'd' },
        Data{ 0x01, 0x04, 'u', 's', 'e', 'r', 0x00 }
    ));

TEST_P(AuthRequestIncomplete, parse)
{
    const auto data = GetParam();
    AuthRequest request;
    EXPECT_EQ(
        Message::ParseStatus::NeedMoreData,
        request.parse(nx::Buffer((char*) data.data(), data.size())).status);
}

INSTANTIATE_TEST_SUITE_P(
    Socks5Messages,
    AuthRequestIncomplete,
    ::testing::Values(
        Data{ 0x01 },
        Data{ 0x01, 0x04 },
        Data{ 0x01, 0x04, 'u', 's' },
        Data{ 0x01, 0x04, 'u', 's', 'e' },
        Data{ 0x01, 0x03, 'u', 's', 'e', 0x01 },
        Data{ 0x01, 0x03, 'u', 's', 'e', 0x02, 'p' }
    ));

TEST(Socks5Messages, AuthRequest_toBuffer)
{
    static constexpr uint8_t data[] = {
        0x01, 0x04, 'u', 's', 'e', 'r', 0x08, 'p', 'a', 's', 's', 'w', 'o', 'r', 'd'
    };
    const nx::Buffer expected((char*) &data, sizeof(data));

    AuthRequest request;
    request.username = "user";
    request.password = "password";

    EXPECT_EQ(expected, request.toBuffer());
}

TEST(Socks5Messages, AuthResponse_parse_status)
{
    static constexpr uint8_t data[] = {
        0x01, 0x01
    };

    const nx::Buffer buffer((char*) &data, sizeof(data));
    AuthResponse response;

    EXPECT_EQ(buffer.size(), response.parse(buffer).size);
    EXPECT_EQ(0x01, response.status);
}

TEST_P(AuthResponseInvalid, parse)
{
    const auto data = GetParam();
    AuthResponse response;
    EXPECT_EQ(
        Message::ParseStatus::Error,
        response.parse(nx::Buffer((char*) data.data(), data.size())).status);
}

INSTANTIATE_TEST_SUITE_P(
    Socks5Messages,
    AuthResponseInvalid,
    ::testing::Values(
        Data{ 0x05, 0x01 },
        Data{ 0x00, 0xFF }
    ));

TEST_P(AuthResponseIncomplete, parse)
{
    const auto data = GetParam();
    AuthResponse response;
    EXPECT_EQ(
        Message::ParseStatus::NeedMoreData,
        response.parse(nx::Buffer((char*) data.data(), data.size())).status);
}

INSTANTIATE_TEST_SUITE_P(
    Socks5Messages,
    AuthResponseIncomplete,
    ::testing::Values(
        Data{ 0x01 }
    ));

TEST(Socks5Messages, AuthResponse_toBuffer)
{
    static constexpr uint8_t data[] = {
        0x01, 0xFF
    };
    const nx::Buffer expected((char*) &data, sizeof(data));

    AuthResponse response(0xFF);

    EXPECT_EQ(expected, response.toBuffer());
}

TEST(Socks5Messages, ConnectRequest_parse_connect_with_domain_address_and_port)
{
    static constexpr uint8_t data[] = {
        0x05, 0x01, 0x00, 0x03, 0x06, 'd', 'o', 'm', 'a', 'i', 'n', 0xab, 0xcd
    };

    const nx::Buffer buffer((char*) &data, sizeof(data));
    ConnectRequest request;

    EXPECT_EQ(buffer.size(), request.parse(buffer).size);
    EXPECT_EQ("domain", request.host);
    EXPECT_EQ(0xabcd, request.port);
}

TEST_P(ConnectRequestInvalid, parse)
{
    const auto data = GetParam();
    ConnectRequest request;
    EXPECT_EQ(
        Message::ParseStatus::Error,
        request.parse(nx::Buffer((char*) data.data(), data.size())).status);
}

INSTANTIATE_TEST_SUITE_P(
    Socks5Messages,
    ConnectRequestInvalid,
    ::testing::Values(
        Data{ 0x04, 0x01, 0x00, 0x03, 0x02, 'd', 'o', 0x01, 0x00 },
        Data{ 0x05, 0x01, 0xF0, 0x03, 0x02, 'd', 'o', 0x01, 0x00 },
        Data{ 0x05, 0x01, 0x00, 0x03, 0x00, 0xab, 0xcd },
        Data{ 0x05, 0x01, 0x00, 0xD0, 0x00, 0xab, 0xcd }
    ));

TEST_P(ConnectRequestIncomplete, parse)
{
    const auto data = GetParam();
    ConnectRequest request;
    EXPECT_EQ(
        Message::ParseStatus::NeedMoreData,
        request.parse(nx::Buffer((char*) data.data(), data.size())).status);
}

INSTANTIATE_TEST_SUITE_P(
    Socks5Messages,
    ConnectRequestIncomplete,
    ::testing::Values(
        Data{ 0x05 },
        Data{ 0x05, 0x02, 0x00, 0x01 },
        Data{ 0x05, 0x01, 0x00, 0x01 },
        Data{ 0x05, 0x01, 0x00, 0x03 },
        Data{ 0x05, 0x01, 0x00, 0x03, 0x01 },
        Data{ 0x05, 0x01, 0x00, 0x03, 0x01, 'a' },
        Data{ 0x05, 0x01, 0x00, 0x03, 0x01, 'a', 0x00 },
        Data{ 0x05, 0x01, 0x00, 0x01, 0x08, 0x08, 0x08 },
        Data{ 0x05, 0x01, 0x00, 0x01, 0x08, 0x08, 0x08, 0x08, 0xab },
        Data{ 0x05, 0x01, 0x00, 0x04, 0x00 }
    ));

TEST(Socks5Messages, ConnectRequest_toBuffer)
{
    static constexpr uint8_t data[] = {
        0x05, 0x01, 0x00, 0x03, 0x06, 'd', 'o', 'm', 'a', 'i', 'n', 0xab, 0xcd
    };
    const nx::Buffer expected((char*) &data, sizeof(data));

    ConnectRequest request;
    request.host = "domain";
    request.port = 0xabcd;

    EXPECT_EQ(expected, request.toBuffer());
}

TEST(Socks5Messages, ConnectRequest_toBuffer_with_different_command)
{
    static constexpr uint8_t data[] = {
        0x05, 0x02, 0x00, 0x03, 0x06, 'd', 'o', 'm', 'a', 'i', 'n', 0xab, 0xcd
    };
    const nx::Buffer expected((char*) &data, sizeof(data));

    ConnectRequest request;
    request.command = 0x02;
    request.host = "domain";
    request.port = 0xabcd;

    EXPECT_EQ(expected, request.toBuffer());
}

TEST(Socks5Messages, ConnectResponse_parse_connect_success_with_domain_address_and_port)
{
    static constexpr uint8_t data[] = {
        0x05, 0x00, 0x00, 0x03, 0x06, 'd', 'o', 'm', 'a', 'i', 'n', 0x12, 0x34
    };

    const nx::Buffer buffer((char*) &data, sizeof(data));
    ConnectResponse response;

    EXPECT_EQ(buffer.size(), response.parse(buffer).size);
    EXPECT_EQ(0x00, response.status);
    EXPECT_EQ("domain", response.host);
    EXPECT_EQ(0x1234, response.port);
}

TEST(Socks5Messages, ConnectResponse_parse_connect_failure_with_ipv4_and_port)
{
    static constexpr uint8_t data[] = {
        0x05, 0x00, 0x00, 0x01, 0x08, 0x08, 0x04, 0x04, 0x12, 0x34
    };

    const nx::Buffer buffer((char*) &data, sizeof(data));
    ConnectResponse response;

    EXPECT_EQ(buffer.size(), response.parse(buffer).size);
    EXPECT_EQ(0x00, response.status);
    EXPECT_EQ("8.8.4.4", response.host);
    EXPECT_EQ(0x1234, response.port);
}

TEST_P(ConnectResponseInvalid, parse)
{
    const auto data = GetParam();
    ConnectResponse response;
    EXPECT_EQ(
        Message::ParseStatus::Error,
        response.parse(nx::Buffer((char*) data.data(), data.size())).status);
}

INSTANTIATE_TEST_SUITE_P(
    Socks5Messages,
    ConnectResponseInvalid,
    ::testing::Values(
        Data{ 0x05, 0x01, 0x01, 0x01 },
        Data{ 0x00, 0xFF, 0x00, 0x01 },
        Data{ 0x05, 0x00, 0x00, 0x00 }
    ));

TEST_P(ConnectResponseIncomplete, parse)
{
    const auto data = GetParam();
    ConnectResponse response;
    EXPECT_EQ(
        Message::ParseStatus::NeedMoreData,
        response.parse(nx::Buffer((char*) data.data(), data.size())).status);
}

INSTANTIATE_TEST_SUITE_P(
    Socks5Messages,
    ConnectResponseIncomplete,
    ::testing::Values(
        Data{ 0x05, 0x00, 0x00, 0x01 },
        Data{ 0x05, 0x00, 0x00, 0x01, 0x01, 0x01, 0x01 },
        Data{ 0x05, 0x00, 0x00, 0x01, 0x01, 0x01, 0x01 },
        Data{ 0x05, 0x01, 0x00, 0x01, 0x01, 0x01, 0x01, 0x01, 0x00 },
        Data{ 0x05, 0x01, 0x00, 0x03, 0x02, 'h' },
        Data{ 0x05, 0x01, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }
    ));

TEST(Socks5Messages, ConnectResponse_toBuffer)
{
    static constexpr uint8_t data[] = {
        0x05, 0x02, 0x00, 0x03, 0x03, 'd', 'o', 'm', 0x12, 0x34
    };
    const nx::Buffer expected((char*) &data, sizeof(data));

    ConnectResponse response(0x02, "dom", 0x1234);

    EXPECT_EQ(expected, response.toBuffer());
}

} // namespace nx::network::socks5::test
