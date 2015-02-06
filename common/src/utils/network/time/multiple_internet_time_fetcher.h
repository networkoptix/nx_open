/**********************************************************
* 5 feb 2015
* a.kolesnikov
***********************************************************/

#ifndef MULTIPLE_INTERNET_TIME_FETCHER_H
#define MULTIPLE_INTERNET_TIME_FETCHER_H

#include "abstract_accurate_time_fetcher.h"

#include <atomic>
#include <memory>
#include <vector>

#include <QtCore/QMutex>


//!Fetches time from multiple time fetchers. If all succeeded than returns mean value
class MultipleInternetTimeFetcher
:
    public AbstractAccurateTimeFetcher
{
public:
    static const qint64 DEFAULT_MAX_DEVIATION_MILLIS = 60*1000;
    
    /*!
        \param maxDeviationMillis Maximum allowed deviation between time values received via different time fetchers
    */
    MultipleInternetTimeFetcher( qint64 maxDeviationMillis = DEFAULT_MAX_DEVIATION_MILLIS );
    virtual ~MultipleInternetTimeFetcher();

    //!Implementation of \a QnStoppable::pleaseStop
    virtual void pleaseStop() override;
    //!Implementation of \a QnJoinable::join
    virtual void join() override;

    //!Implementation of AbstractAccurateTimeFetcher
    virtual bool getTimeAsync( std::function<void(qint64, SystemError::ErrorCode)> handlerFunc ) override;

    void addTimeFetcher( std::unique_ptr<AbstractAccurateTimeFetcher> timeFetcher );

private:
    struct TimeFetcherContext
    {
        enum State
        {
            init,
            working,
            done
        };

        std::unique_ptr<AbstractAccurateTimeFetcher> timeFetcher;
        qint64 utcMillis;
        SystemError::ErrorCode errorCode;
        State state;

        TimeFetcherContext();
    };

    qint64 m_maxDeviationMillis;
    std::vector<std::unique_ptr<TimeFetcherContext>> m_timeFetchers;
    mutable QMutex m_mutex;
    std::atomic<size_t> m_awaitedAnswers;
    bool m_terminated;
    std::function<void(qint64, SystemError::ErrorCode)> m_handlerFunc;

    void timeFetchingDone(
        TimeFetcherContext* ctx,
        qint64 utcMillis,
        SystemError::ErrorCode errorCode );
};

#endif //MULTIPLE_INTERNET_TIME_FETCHER_H
