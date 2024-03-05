// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "disk_utils_win.h"

#include <QtCore/QVector>

#include <nx/utils/log/log.h>

namespace nx::monitoring {

namespace {

QVector<WCHAR> prepareDriveNameBuffer()
{
    QVector<WCHAR> result;

    const DWORD bufferSize = GetLogicalDriveStringsW(0, nullptr);
    if (!bufferSize)
    {
        NX_ERROR(NX_SCOPE_TAG, "GetLogicalDriveStringsW failed");
        return result;
    }
    result.resize(bufferSize);

    if (!GetLogicalDriveStringsW(static_cast<DWORD>(result.size()), result.data()))
    {
        NX_ERROR(NX_SCOPE_TAG, "GetLogicalDriveStringsW failed");
        return result;
    }

    if (nx::log::isToBeLogged(nx::log::Level::verbose))
    {
        const auto out = QString::fromWCharArray(result.data(), result.size()).replace('\0', ' ');
        NX_VERBOSE(NX_SCOPE_TAG, "GetLogicalDriveStringsW returned '%1'", out);
    }

    return result;
}

} // namespace

QStringList getDriveNames()
{
    const auto nameBuffer = prepareDriveNameBuffer();
    const auto* currentPositionPtr = nameBuffer.data();
    QStringList result;
    while (*currentPositionPtr)
    {
        const auto driveName = QString::fromWCharArray(currentPositionPtr);
        result.append(driveName);
        currentPositionPtr += driveName.size() + 1;
    }

    return result;
}

WinHandlePtr getDriveHandle(const QString& driveName)
{
    const QString driveSysString = QString("\\\\.\\%1:").arg(driveName[0]);
    auto res = new HANDLE(CreateFile(
        reinterpret_cast<LPCWSTR>(driveSysString.data()),
        FILE_READ_ATTRIBUTES,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr, OPEN_EXISTING, 0, nullptr));

    return WinHandlePtr(
        res,
        [](HANDLE* h)
        {
            CloseHandle(*h);
            delete h;
        });
}

} // namespace nx::monitoring
