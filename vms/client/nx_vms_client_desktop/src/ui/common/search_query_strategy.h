// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/


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

    QString query() const;

signals:
    void queryUpdated(const QString &query);

public slots:
    void changeQuery(const QString &query);

    void changeQueryForcibly(const QString &query);

private:
    class Impl;
    Impl * const m_impl;
};
