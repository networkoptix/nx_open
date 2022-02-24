// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QString>

class QnNovLauncher
{
public:
    enum class ErrorCode
    {
        Ok,
        NoTargetFileAccess,
        WriteFileError,
        WriteMediaError,
        WriteIndexError
    };

    enum class ExportMode
    {
        withVideo,
        standaloneClient
    };

    // Creates valid (in the sense of layout::identifyFile) nov file containing client and exe header.
    static ErrorCode createLaunchingFile(
        const QString& dstName,
        ExportMode exportMode = ExportMode::withVideo);
};
