/**********************************************************
* 10 jul 2014
* a.kolesnikov
***********************************************************/

#ifndef ABSTRACT_ACCURATE_TIME_FETCHER_H
#define ABSTRACT_ACCURATE_TIME_FETCHER_H

#include <functional>

#include <QtGlobal>

#include "utils/common/joinable.h"
#include "utils/common/stoppable.h"
#include "utils/common/systemerror.h"


//!Abstract interface for class performing fetching accurate current time from some public server
class AbstractAccurateTimeFetcher
:
    public QnStoppable,
    public QnJoinable
{
public:
    virtual ~AbstractAccurateTimeFetcher() {}

    //!Initiates asynchronous time request operation
    /*!
        Upon completion \a handlerFunc will be called. Following parameters are passed:\n
        - \a qint64 on success UTC millis from epoch. On failure -1
        - \a SystemError::ErrorCode last system error code. \a SystemError::noError in case of success
        \note Implementation is NOT REQUIRED to support performing multiple simultaneous operations
        \return \a true if request issued successfully, otherwise \a false
    */
    virtual bool getTimeAsync( std::function<void(qint64, SystemError::ErrorCode)> handlerFunc ) = 0;
};

#endif  //ABSTRACT_ACCURATE_TIME_FETCHER_H
