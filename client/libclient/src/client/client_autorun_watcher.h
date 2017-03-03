#pragma once

#include <QObject>

class QnClientAutoRunWatcher: public QObject
{
    Q_OBJECT;
public:
    QnClientAutoRunWatcher(QObject* parent = NULL);

    bool isAutoRun() const;
    void setAutoRun(bool enabled);

private:
    QString autoRunKey() const;
    QString autoRunPath() const;
};
