// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "symmetrical.h"

#include <array>
#include <stdint.h>
#include <cstring> //< CBC mode, for memset

#include <nx/utils/uuid.h>
#include <nx/utils/log/assert.h>
#include <nx/utils/thread/mutex.h>

extern "C" {
#include <tiny_aes_c/aes.h>
} // extern "C"

namespace nx::crypt {

namespace detail {

const uint8_t iv[] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c,
    0x0d, 0x0e, 0x0f};

static nx::Mutex& stateMutex()
{
    static nx::Mutex mtx;
    return mtx;
}

static constexpr size_t kKeySize = 16;

// Hardcoded random key, generated from guid.
static const QByteArray kMask = QByteArray::fromHex("4453D6654C634636990B2E5AA69A1312");
const int kMaskSize = kMask.size();

using KeyType = std::array<uint8_t, kKeySize>;

KeyType keyFromByteArray(const QByteArray& data)
{
    KeyType result;
    for (int i = 0; i < (int)result.size(); ++i)
    {
        if (i < data.size())
            result[(size_t)i] = (uint8_t)data[i];
        else if (i == data.size())
            result[i] = (uint8_t)(0x1);
        else
            result[i] = result[i - 1] + 1;
    }
    return result;
}

} // namespace detail

QByteArray generateAesExtraKey()
{
    return nx::Uuid::createUuid().toRfc4122();
}

QByteArray encodeSimple(const QByteArray& data, const QByteArray& extraKey)
{
    QByteArray mask = detail::kMask;
    for (int i = 0; i < qMin(mask.size(), extraKey.size()); ++i)
        mask[i] = mask[i] ^ extraKey[i];

    QByteArray result = data;
    for (int i = 0; i < result.size(); ++i)
        result.data()[i] ^= mask.data()[i % detail::kMaskSize];
    return result;
}

QByteArray encodeAES128CBC(const QByteArray& data, const std::array<uint8_t, 16>& key)
{
    if (data.isEmpty())
        return QByteArray();

    NX_MUTEX_LOCKER lock(&detail::stateMutex());
    const QByteArray* pdata = &data;
    QByteArray dataCopy;
    const int padSize = 16 - data.size() % 16;
    if (padSize != 0)
    {
        dataCopy = data;
        dataCopy.append(QByteArray(padSize, 0));
        pdata = &dataCopy;
    }
    QByteArray result;
    result.resize(pdata->size());
    AES128_CBC_encrypt_buffer(
        (uint8_t*)result.data(),
        (uint8_t*)pdata->data(),
        pdata->size(),
        key.data(),
        detail::iv);
    return result;
}

QByteArray decodeAES128CBC(const QByteArray& data, const std::array<uint8_t, 16>& key)
{
    if (data.isEmpty())
        return QByteArray();

    NX_MUTEX_LOCKER lock(&detail::stateMutex());
    if (data.size() % 16 != 0)
        return QByteArray();

    QByteArray result;
    result.resize(data.size());
    AES128_CBC_decrypt_buffer(
        (uint8_t*)result.data(),
        (uint8_t*)data.data(),
        data.size(),
        key.data(),
        detail::iv);
    return result.left(result.indexOf((char)0));
}

QByteArray encodeAES128CBC(const QByteArray& data, const QByteArray& key)
{
    const QByteArray actualKey = key.isEmpty() ? detail::kMask : key;
    return encodeAES128CBC(data, detail::keyFromByteArray(actualKey));
}

QByteArray decodeAES128CBC(const QByteArray& data, const QByteArray& key)
{
    const QByteArray actualKey = key.isEmpty() ? detail::kMask : key;
    return decodeAES128CBC(data, detail::keyFromByteArray(actualKey));
}

QString encodeHexStringFromStringAES128CBC(const QString& s, const QByteArray& key)
{
    return QString::fromLatin1(encodeAES128CBC(s.toUtf8(), key).toHex());
}

QString decodeStringFromHexStringAES128CBC(const QString& s, const QByteArray& key)
{
    return QString::fromUtf8(decodeAES128CBC(QByteArray::fromHex(s.toLatin1()), key));
}

} // namespace nx::crypt
