#pragma once

#include <QtCore/QString>

class AVStringError
{
public:
    AVStringError();
    ~AVStringError();

    void setAvError(const QString& error);
    QString getAvError();

    bool updateIfError(int code);
    bool hasError();

private:
    QString m_lastError;
};

