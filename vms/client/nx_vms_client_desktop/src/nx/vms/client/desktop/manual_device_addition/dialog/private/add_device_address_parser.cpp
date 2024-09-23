// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "add_device_address_parser.h"

#include <regex>
#include <string>

#include <nx/utils/std_string_utils.h>

namespace nx::vms::client::desktop::manual_device_addition {

static AddDeviceSearchAddress extractIpAddressRange(const QString& address);
static AddDeviceSearchAddress extractDomainAndPort(const QString& address);
static void fillAddressRangeFromSubnetMaskNumBits(AddDeviceSearchAddress& address,
    const std::string& subnetMaskNumBits,
    const std::string& port,
    int portPosition,
    int portLength);
static std::optional<int> extractPort(const std::string& portStr);
static std::optional<int> extractSubnetMaskNumBits(const std::string& subnetMaskNumBits);

AddDeviceSearchAddress parseDeviceAddress(const QString& address)
{
    auto result = extractIpAddressRange(address);
    if (result.valid())
        return result;

    // IP address wasn't matched, do a very relaxed port matching inside a domain name.
    return extractDomainAndPort(address);
}

static AddDeviceSearchAddress extractIpAddressRange(const QString& address)
{
    static const std::regex kAddressRangePattern(
        R"(^\s*([^:.]+:\/\/)?)"         // optional scheme (allow anything followed by a "://")
        R"(((?:\d{1,3}\.){3}\d{1,3}))"  // initial IP address
        R"((?::(\d+))?)"                // optionally followed by a port number
        R"((?:(?:(?:\s*-+\s*)"          // dashes between the two addresses
        R"(((?:\d{1,3}\.){3}\d{1,3})|)" // it could be and address
        R"((?:\/(\d{1,3})(?::(\d+))?)))" // or number of bits in subnet mask
        R"((?::\d+)?))?\s*)");          // second address can have a port, but it will be ignored

    constexpr int kSchemeIndex = 1;
    constexpr int kStartAddressIndex = 2;
    constexpr int kEndAddressIndex = 4;
    constexpr int kPortIndex = 3;
    constexpr int kSubnetMaskNumBitsIndex = 5;
    constexpr int kAddressRangePortIndex = 6;

    AddDeviceSearchAddress result;

    std::smatch matches;
    const auto& input = address.toStdString();

    if (std::regex_match(input, matches, kAddressRangePattern))
    {
        if (matches[kStartAddressIndex].matched)
            result.startAddress = QString::fromStdString(
                matches[kSchemeIndex].str() + matches[kStartAddressIndex].str());
        if (matches[kEndAddressIndex].matched)
            result.endAddress = QString::fromStdString(
                matches[kSchemeIndex].str() + matches[kEndAddressIndex].str());
        if (matches[kPortIndex].matched)
        {
            auto port = extractPort(matches[kPortIndex]);
            if (port.has_value())
            {
                result.port = port;
                result.portIndex = matches.position(kPortIndex);
                result.portLength = matches.length(kPortIndex);
            }
        }
        if (matches[kSubnetMaskNumBitsIndex].matched)
        {
            fillAddressRangeFromSubnetMaskNumBits(result,
                matches[kSubnetMaskNumBitsIndex],
                matches[kAddressRangePortIndex],
                matches.position(kAddressRangePortIndex),
                matches.length(kAddressRangePortIndex));
        }
    }

    return result;
}

static AddDeviceSearchAddress extractDomainAndPort(const QString& address)
{
    static const std::regex kDomainPortPattern(
        R"(^\s*((?:[^:]+:\/\/)?)"   // scheme (allow anything followed by a "://")
        R"([^.:\n]+(?:\.[^.:\n]+)*)" // host name, unspecified number of dot-separated parts
        R"((?::(\d+))?)"            // optionally followed by a port
        R"((?:\/\S*)?)\s*)");       // optionally rest of the URL

    constexpr int kWholeMatchNoWhitespaceIndex = 1;
    constexpr int kPortIndex = 2;

    AddDeviceSearchAddress result;

    std::smatch matches;
    const auto& input = address.toStdString();

    if (std::regex_match(input, matches, kDomainPortPattern))
    {
        auto port = extractPort(matches[kPortIndex]);
        if (port.has_value())
        {
            result.port = port;
            result.portIndex = matches.position(kPortIndex);
            result.portLength = matches.length(kPortIndex);
        }
    }

    // At this point accept anything as a start address.
    result.startAddress = QString::fromStdString(matches[kWholeMatchNoWhitespaceIndex].str());

    return result;
}

static void fillAddressRangeFromSubnetMaskNumBits(AddDeviceSearchAddress& address,
    const std::string& subnetMaskNumBits,
    const std::string& portStr,
    int portPosition,
    int portLength)
{
    auto maskNumBits = extractSubnetMaskNumBits(subnetMaskNumBits);
    if (NX_ASSERT(address.startAddress.has_value()) && maskNumBits.has_value())
    {
        constexpr int kNumOctets = 4;
        auto octetList = address.startAddress->split('.');
        if (NX_ASSERT(octetList.size() == kNumOctets))
        {
            constexpr int kNumBitsPerOctet = 8;
            constexpr int kMaxCharsPerOctet = 4;

            address.startAddress.emplace();
            address.startAddress->reserve(kNumOctets * kMaxCharsPerOctet);
            address.endAddress.emplace();
            address.endAddress->reserve(kNumOctets * kMaxCharsPerOctet);

            for (int i = 0; i < kNumOctets; i++)
            {
                int octetMaskNumBits = std::min(*maskNumBits, kNumBitsPerOctet);
                uint8_t octetMask = 255 << (kNumBitsPerOctet - octetMaskNumBits);

                uint8_t octet = octetList[i].toInt();
                uint8_t startOctet = octet & octetMask;
                uint8_t endOctet = octet | ~octetMask;

                *address.startAddress += QString::number(startOctet);
                *address.startAddress += '.';
                *address.endAddress += QString::number(endOctet);
                *address.endAddress += '.';

                *maskNumBits -= octetMaskNumBits;
            }

            address.startAddress->removeLast();
            address.endAddress->removeLast();
        }

        auto port = extractPort(portStr);
        if (port.has_value())
        {
            address.port = port;
            address.portIndex = portPosition;
            address.portLength = portLength;
        }
    }
}

static std::optional<int> extractPort(const std::string& portStr)
{
    if (portStr.empty())
        return {};

    std::size_t pos;
    int port = nx::utils::stoi(portStr, &pos);

    constexpr int kMinPort = 0;
    constexpr int kMaxPort = 65'535;

    if (pos != portStr.size() || port < kMinPort || port > kMaxPort)
        return {};

    return port;
}

static std::optional<int> extractSubnetMaskNumBits(const std::string& subnetMaskNumBits)
{
    if (subnetMaskNumBits.empty())
        return {};

    std::size_t pos;
    int numBits = nx::utils::stoi(subnetMaskNumBits, &pos);

    constexpr int kMinSubnetValue = 0;
    constexpr int kMaxSubnetValue = 32;

    if (pos != subnetMaskNumBits.size() || numBits < kMinSubnetValue || numBits > kMaxSubnetValue)
        return {};

    return numBits;
}

} // namespace nx::vms::client::desktop
