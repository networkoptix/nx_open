// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QDir>
#include <QtCore/QString>

#include <nx/utils/move_only_func.h>

namespace nx::utils::debug {

using OnAboutToBlockHandler = nx::MoveOnlyFunc<void()>;

/**
 * @return The previous handler
 */
NX_UTILS_API OnAboutToBlockHandler setOnAboutToBlockHandler(
    OnAboutToBlockHandler handler);

NX_UTILS_API void onAboutToBlock();

enum class FormatFilePattern
{
    desktopClient,
    mediaServer,
};
/*
 * Returns full path to the file name withing the given directory.
 * If the file name is absolute, it will be returned as is.
 * Replaces %T and %P in the file name with the following values:
 *   %T - current date/time in format yyyy-MM-dd_HH-mm-ss-zzz
 *   %P - process id
 * If formatPattern is desktopClient then replaces %N and %V in the file name with
 * the following values:
 *   %N - client name
 *   %V - client version
 */
NX_UTILS_API QString formatFileName(
    QString name, FormatFilePattern formatPattern, const QDir& defaultDir = {});

/*
 * Returns full path to the file name withing the given directory.
 * If the file name is absolute, it will be returned as is.
 * Replaces %T, %P, %N and %V in the file name with the following values:
 *   %T - current date/time in format yyyy-MM-dd_HH-mm-ss-zzz
 *   %P - process id
 *   %N - client name
 *   %V - client version
 */
NX_UTILS_API QString formatFileName(QString name, const QDir& defaultDir = {});

} // namespace nx::utils::debug
