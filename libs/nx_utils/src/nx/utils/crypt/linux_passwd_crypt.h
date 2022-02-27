// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QByteArray>

namespace nx::utils {

static constexpr size_t kLinuxCryptSaltLength = 8;

QByteArray NX_UTILS_API generateSalt(int length);

/** 
 * Calculates sha512 hash suitable for /etc/shadow file.
 */
QByteArray NX_UTILS_API linuxCryptSha512(const QByteArray& password, const QByteArray& salt);


/**
 * Currently implemented on linux only. Modifies /etc/shadow. digest MUST be suitable for a shadow file
 * NOTE: on linux process MUST have root priviledge to be able to modify /etc/shadow
 */
bool NX_UTILS_API setRootPasswordDigest(const QByteArray& userName, const QByteArray& digest);

} // namespace nx::utils
