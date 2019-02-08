#pragma once

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

    // Creates valid (in the sense of layout::identifyFile) nov file containing client and exe header.
    static ErrorCode createLaunchingFile(const QString& dstName, const QString& novFileName = QString());
};
