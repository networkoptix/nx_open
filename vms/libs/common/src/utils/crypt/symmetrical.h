#pragma once

#include <array>

#include <QtCore/QByteArray>

namespace nx::utils {

/**
 * Generate random 16-bytes length key (sufficient for AES cryptography).
 */
QByteArray generateAesExtraKey();

/**
 * Encode data with the provided key using XOR cypher.
 */
QByteArray encodeSimple(const QByteArray& data, const QByteArray& extraKey = QByteArray());

/**
 * Encode data with the provided key using AES symmetrical cryptography.
 * NOTE: data to be encoded shouldn't contain '\0' because decrypted data is stripped at the first
 * '\0' symbol. E.g. these below work fine with utf-8 strings, but not with utf-16(32) ones.
 */
QByteArray encodeAES128CBC(const QByteArray& data, const std::array<uint8_t, 16>& key);

/**
 * Decode data with the provided key using AES symmetrical cryptography.
 * NOTE: data to be encoded shouldn't contain '\0' because decrypted data is stripped at the first
 * '\0' symbol. E.g. these below work fine with utf-8 strings, but not with utf-16(32) ones.
 */
QByteArray decodeAES128CBC(const QByteArray& data, const std::array<uint8_t, 16>& key);

/**
 * Encode data with the provided key using AES symmetrical cryptography. If key is less than 16
 * bytes, it is completed with 0x01, 0x02, ... sequence. Otherwise it is stripped off to 16 bytes.
 * If key is not provided at all, default hardcoded key is used.
 * NOTE: data to be encoded shouldn't contain '\0' because decrypted data is stripped at the first
 * '\0' symbol. E.g. these below work fine with utf-8 strings, but not with utf-16(32) ones.
 */
QByteArray encodeAES128CBC(const QByteArray& data, const QByteArray& key = QByteArray());

/**
 * Decode data with the provided key using AES symmetrical cryptography. If key is less than 16
 * bytes, it is completed with 0x01, 0x02, ... sequence. Otherwise it is stripped off to 16 bytes.
 * If key is not provided at all, default hardcoded key is used.
  * NOTE: data to be encoded shouldn't contain '\0' because decrypted data is stripped at the first
 * '\0' symbol. E.g. these below work fine with utf-8 strings, but not with utf-16(32) ones.
 */
QByteArray decodeAES128CBC(const QByteArray& data, const QByteArray& key = QByteArray());

/**
 * Encode string with the provided key using AES symmetrical cryptography. If key is less than 16
 * bytes, it is completed with 0x01, 0x02, ... sequence. Otherwise it is stripped off to 16 bytes.
 * If key is not provided at all, default hardcoded key is used.
 */
QString encodeHexStringFromStringAES128CBC(const QString& s, const QByteArray& key = QByteArray());

/**
 * Encode string with the provided key using AES symmetrical cryptography. If key is less than 16
 * bytes, it is completed with 0x01, 0x02, ... sequence. Otherwise it is stripped off to 16 bytes.
 * If key is not provided at all, default hardcoded key is used.
 */
QString decodeStringFromHexStringAES128CBC(const QString& s, const QByteArray& key = QByteArray());

} // namespace nx::utils
