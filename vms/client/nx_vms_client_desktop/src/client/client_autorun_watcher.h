// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QObject>

class QnClientAutoRunWatcher: public QObject
{
    Q_OBJECT;
public:
    QnClientAutoRunWatcher(QObject* parent = nullptr);

    bool isAutoRun() const;
    void setAutoRun(bool enabled);

private:
    QString autoRunKey() const;
    QString autoRunPath() const;
};
