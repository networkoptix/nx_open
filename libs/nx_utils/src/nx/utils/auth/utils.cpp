// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "utils.h"

#include <openssl/hmac.h>
#include <QCryptographicHash>

#include <nx/utils/type_utils.h>

namespace nx::utils::auth {

char encodeFiveBits(const char c)
{
    if (c < 26)
        return 'A' + c;
    if (c >= 26 && c <= 31)
        return '2' + c - 26;
    NX_ASSERT(false);
    return 0;
}

std::tuple<bool, char> decodeBase32Char(const char c)
{
    if (('A' <= c) && (c <= 'Z'))
        return {true, c - 'A'};
    if (('2' <= c) && (c <= '7'))
        return {true, c - '2' + 26};

    return {false, 0};
}

std::string encodeBlock(const std::string_view str)
{
    NX_ASSERT(str.size() <= 5);
    int64_t buf = 0;
    for (const auto c: str)
    {
        buf |= c;
        buf = buf << 8;
    }
    buf = buf >> 8;

    const int kResponseLen = 8;
    int curResponseLen = 8;
    if (str.size() < 5)
    {
        curResponseLen = str.size() * 8 / 5 + 1;
        buf = buf << (5 - (str.size() * 8 % 5));
    }
    std::string result;
    result.reserve(8);

    for (int i = 0; i < curResponseLen; ++i)
    {
        result.push_back(encodeFiveBits(buf & 0x1F));
        buf = buf >> 5;
    }
    std::reverse(result.begin(), result.end());
    result.resize(kResponseLen);
    std::fill(result.begin() + curResponseLen, result.end(), '=');
    return result;
}

std::string encodeToBase32(const std::string& str)
{
    int resultLenght = str.size() / 5 * 8;
    if (str.size() % 5 != 0)
        resultLenght += 8;
    std::string result;
    result.reserve(resultLenght);
    for (int i = 0; i <= (int) str.size() - 5; i += 5)
        result += encodeBlock(std::string_view(str.c_str() + i, 5));

    if (str.size() % 5)
    {
        result += encodeBlock(
            std::string_view(str.c_str() + str.size() - str.size() % 5, str.size() % 5));
    }
    return result;
}

std::tuple<bool, std::string> decodeBlock(const std::string_view str)
{
    if (str.size() != 8)
        return {false, ""};

    uint64_t buf = 0;
    int charNum = 0;
    for (; charNum < 8; ++charNum)
    {
        if (str[charNum] == '=')
            break;
        const auto [success, decodedChar] = decodeBase32Char(str[charNum]);
        if (!success)
            return {false, ""};
        buf = buf | decodedChar;
        buf = buf << 5;
    }
    if (charNum != 2 && charNum != 4 && charNum != 5 && charNum != 7 && charNum != 8)
        return {false, ""};
    int shift = charNum * 5 % 8; //< Number of bits added to form an integral number of 5-bit groups.
    int resultLen = charNum * 5 / 8;
    buf = buf >> 5 >> shift;
    std::string result;
    result.reserve(resultLen);
    for (int i = 0; i < resultLen; ++i)
    {
        result.push_back((unsigned char) buf);
        buf = buf >> 8;
    }
    std::reverse(result.begin(), result.end());
    return {true, result};
}

std::tuple<bool, std::string> decodeBase32(const std::string& str)
{
    if (str.size() % 8 != 0)
        return {false, ""};
    int resultLenght = str.size() / 8 * 5;
    std::string result;
    result.reserve(resultLenght);
    for (int i = 0; i <= (int) str.size() - 8; i += 8)
    {
        auto [success, blockResult] = decodeBlock(std::string_view(str.c_str() + i, 8));
        if (!success)
            return {false, ""};
        result += std::move(blockResult);
    }

    return {true, result};
}

Buffer hmacSha1(const std::string_view& key, const std::string_view& baseString)
{
    return hmacSha1(key, std::vector<std::string_view>{baseString});
}

Buffer hmacSha1(
    const std::string_view& key,
    const std::vector<std::string_view>& messageParts)
{
    unsigned int len = 0;
    Buffer result(20, 0);

    auto ctx = utils::wrapUnique(HMAC_CTX_new(), &HMAC_CTX_free);
    HMAC_Init_ex(ctx.get(), key.data(), key.size(), EVP_sha1(), nullptr);
    for (const auto& part: messageParts)
        HMAC_Update(ctx.get(), (const unsigned char*) part.data(), part.size());
    HMAC_Final(ctx.get(), (unsigned char*) result.data(), &len);

    result.resize(len);
    return result;
}

Buffer hmacSha256(const std::string_view& key, const std::string_view& baseString)
{
    unsigned int len;
    Buffer result(32, 0);

    auto ctx = utils::wrapUnique(HMAC_CTX_new(), &HMAC_CTX_free);
    HMAC_Init_ex(ctx.get(), key.data(), key.size(), EVP_sha256(), nullptr);
    HMAC_Update(ctx.get(), (const unsigned char*) baseString.data(), baseString.size());
    HMAC_Final(ctx.get(), (unsigned char*) &*result.begin(), &len);

    result.resize(len);
    return result;
}


} // namespace nx::utils::auth
