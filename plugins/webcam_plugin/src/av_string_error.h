#pragma once

#include <QtCore/QString>

class AVStringError
{
public:
    AVStringError();

    void setAvError(const QString& error);
    QString avErrorString() const;

    void setAvErrorCode(int errorCode);
    int errorCode() const;

    bool updateIfError(int code);
    bool hasError() const;

private:
    QString m_lastError;
    int m_errorCode;
};

