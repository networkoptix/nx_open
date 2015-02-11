
#pragma once

#include <QtCore/QObject>

class QnSearchQueryStrategy : public QObject
{
    Q_OBJECT
public:
    enum { kMinSymbolsCount = 0};
    enum { kNoTimeout = 0 };

    explicit QnSearchQueryStrategy(QObject *parent = nullptr
        , int minSymbolsCount = kMinSymbolsCount
        , int delayOnTypingDurationMs = kNoTimeout);

    virtual ~QnSearchQueryStrategy();

signals:
    void updateQuery(const QString& query);

public slots:
    void queryChanged(const QString& query);

    void forciblyQueryChanged(const QString& query);

private:
    class Impl;
    Impl * const m_impl;
};