// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "desktop_secure_storage.h"

#if !defined(Q_OS_IOS) && !defined(Q_OS_ANDROID)

#include <QtCore/QStandardPaths>
#include <QtCore/QDir>

#include <nx/utils/cryptographic_hash.h>
#include <nx/utils/log/log.h>

namespace nx::vms::client::mobile {

namespace {

QString dataPath(std::string_view key)
{
    return NX_FMT("storage/data/%1.json", key);
}

QString filePath(std::string_view key)
{
    const auto hash = nx::utils::sha1(key);
    const auto hex = QByteArray::fromRawData(hash.data(), hash.size()).toHex();

    return NX_FMT("storage/%1/%2", hex[0], hex);
}

std::optional<QByteArray> readWholeFile(const QString& path, bool binary)
{
    if (path.isEmpty())
        return {};

    if (QFile file(path); file.open(binary ? QFile::ReadOnly : QFile::ReadOnly | QFile::Text))
        return file.readAll();

    return {};
}

} // namespace

std::optional<std::string> DesktopSecureStorage::load(const std::string& key) const
{
    const auto filename = QStandardPaths::locate(
        QStandardPaths::AppLocalDataLocation, dataPath(key), QStandardPaths::LocateFile);

    const auto body = readWholeFile(filename, false);

    return body.transform([](const auto& bytes) { return bytes.toStdString(); });
}

void DesktopSecureStorage::save(const std::string& key, const std::string& value)
{
    const auto path = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)
        .append('/').append(dataPath(key));

    QFileInfo(path).absoluteDir().mkpath(".");

    if (QFile file(path); file.open(QFile::WriteOnly | QFile::Truncate | QFile::Text))
        file.write(value.data(), value.size());
}

std::optional<std::vector<std::byte>> DesktopSecureStorage::loadFile(const std::string& key) const
{
    const auto path = QStandardPaths::locate(
        QStandardPaths::AppLocalDataLocation, filePath(key), QStandardPaths::LocateFile);

    return readWholeFile(path, true).transform(
        [](const auto& bytes)
        {
            return std::vector<std::byte>(
                (const std::byte*) bytes.constData(),
                (const std::byte*) (bytes.constData() + bytes.size()));
        });
}

void DesktopSecureStorage::saveFile(const std::string& key, const std::vector<std::byte>& data)
{
    const auto path = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)
        .append('/').append(filePath(key));

    QFileInfo(path).absoluteDir().mkpath(".");

    if (QFile file(path); file.open(QFile::WriteOnly | QFile::Truncate))
        file.write((const char *) data.data(), data.size());
}

void DesktopSecureStorage::removeFile(const std::string& key)
{
    if (const auto path = QStandardPaths::locate(
        QStandardPaths::AppLocalDataLocation, filePath(key), QStandardPaths::LocateFile);
        !path.isEmpty())
    {
        QFile::remove(path);
    }
}

} // namespace nx::vms::client::mobile

#endif
