// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "abstract_stored_file_manager.h"

#include "../detail/call_sync.h"

namespace ec2 {

ErrorCode AbstractStoredFileManager::getStoredFileSync(
    const QString& fileName, QByteArray* outFileData)
{
    return detail::callSync(
        [&](auto handler)
        {
            getStoredFile(fileName, std::move(handler));
        },
        outFileData);
}

ErrorCode AbstractStoredFileManager::addStoredFileSync(
    const QString& fileName, const QByteArray& fileData)
{
    return detail::callSync(
        [&](auto handler)
        {
            addStoredFile(fileName, fileData, std::move(handler));
        });
}

ErrorCode AbstractStoredFileManager::deleteStoredFileSync(const QString& fileName)
{
    return detail::callSync(
        [&](auto handler)
        {
            deleteStoredFile(fileName, std::move(handler));
        });
}

ErrorCode AbstractStoredFileManager::listDirectorySync(
    const QString& directoryName, QStringList* outNameList)
{
    return detail::callSync(
        [&](auto handler)
        {
            listDirectory(directoryName, std::move(handler));
        },
        outNameList);
}

} // namespace ec2
