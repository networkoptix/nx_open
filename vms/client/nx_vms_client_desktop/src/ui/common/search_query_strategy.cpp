// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "search_query_strategy.h"

#include <boost/noncopyable.hpp>

#include <QtCore/QTimerEvent>

namespace
{
    enum { kNoTimerId = 0 };
}

class QnSearchQueryStrategy::Impl : public QObject
{
public:
    Impl(QnSearchQueryStrategy *owner
        , int minSymbolsCount
        , int delayOnTypingDurationMs);

    virtual ~Impl();

    void changeQuery(const QString &currentQuery
        , bool forcibly);

    QString currentQuery() const;

private:
    void updateQuery(const QString &currentQuery
        , bool forcibly);

private:
    void timerEvent(QTimerEvent *event) override;

    void killDelayTimer();

private:
    const int m_minSymbolsCount;
    const int m_delayOnTypingDurationMs;
    QnSearchQueryStrategy &m_owner;

    QString m_currentQuery;
    int m_delayTimerId;
};

QnSearchQueryStrategy::Impl::Impl(QnSearchQueryStrategy *owner
    , int minSymbolsCount
    , int delayOnTypingDurationMs)
    : QObject(owner)
    , m_minSymbolsCount(minSymbolsCount)
    , m_delayOnTypingDurationMs(delayOnTypingDurationMs)
    , m_owner(*owner)
    , m_currentQuery()
    , m_delayTimerId(kNoTimerId)
{
}

QnSearchQueryStrategy::Impl::~Impl()
{
}

void QnSearchQueryStrategy::Impl::changeQuery(const QString &query
    , bool forcibly)
{
    killDelayTimer();

    if (forcibly)
    {
        updateQuery(query, true);
        return;
    }

    m_currentQuery = query.trimmed();
    if (m_delayOnTypingDurationMs > kNoTimeout)
    {
        m_delayTimerId = startTimer(m_delayOnTypingDurationMs);
    }

    if ((m_delayTimerId == kNoTimerId) || (m_delayOnTypingDurationMs <= kNoTimeout))
    {
        updateQuery(m_currentQuery, false);
    }
}

QString QnSearchQueryStrategy::Impl::currentQuery() const
{
    return m_currentQuery;
}

void QnSearchQueryStrategy::Impl::updateQuery(const QString &query
    , bool forcibly)
{
    if (forcibly || (m_currentQuery.size() >= m_minSymbolsCount) || m_currentQuery.trimmed().isEmpty())
    {
        m_owner.queryUpdated(query);
    }
}

void QnSearchQueryStrategy::Impl::timerEvent(QTimerEvent *event)
{
    if (event->timerId() == m_delayTimerId)
    {
        killDelayTimer();
        updateQuery(m_currentQuery, false);
    }
}

void QnSearchQueryStrategy::Impl::killDelayTimer()
{
    if (m_delayTimerId != kNoTimerId)
    {
        killTimer(m_delayTimerId);
        m_delayTimerId = kNoTimerId;
    }
}

///

QnSearchQueryStrategy::QnSearchQueryStrategy(QObject *parent
    , int minSymbolsCount
    , int delayOnTypingDurationMs)
    : QObject(parent)
    , m_impl(new Impl(this, minSymbolsCount, delayOnTypingDurationMs))
{
}

QnSearchQueryStrategy::~QnSearchQueryStrategy()
{
}

QString QnSearchQueryStrategy::query() const
{
    return m_impl->currentQuery();
}

void QnSearchQueryStrategy::changeQuery(const QString &query)
{
    m_impl->changeQuery(query, false);
}

void QnSearchQueryStrategy::changeQueryForcibly(const QString &query)
{
    m_impl->changeQuery(query, true);
}
