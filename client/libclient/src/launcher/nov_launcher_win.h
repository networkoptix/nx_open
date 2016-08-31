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

    static ErrorCode createLaunchingFile(const QString& dstName, const QString& novFileName = QString());
};
