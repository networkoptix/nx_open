#pragma once

#include <QtCore/QString>

class AVStringError
{
public:
    AVStringError();
    ~AVStringError();

    void setAvError(const QString& error);
    QString avErrorString();

    bool updateIfError(int code);
    bool hasError();

private:
    QString m_lastError;
};

