/**********************************************************
* 28 jan 2013
* a.kolesnikov
***********************************************************/

#ifndef QNSTOPPABLE_H
#define QNSTOPPABLE_H

#include <functional>


//!Abstract class providing interface to stop doing anything without object destruction
class QN_EXPORT QnStoppable
{
public:
    virtual ~QnStoppable() {}

    virtual void pleaseStop() = 0;
};

//!Abstract interface to interrupt asynchronous operation with completion notification
class QN_EXPORT QnStoppableAsync
{
public:
    virtual ~QnStoppableAsync() {}

    /*!
        \param completionHandler Executed when asynchronous operation is interrupted. For example, in case with async socket operations, 
            \a completionHandler is triggered when socket completion handler returned or will never be called.
            Allowed to be \a null
        \note If operation is already stopped it is allowed for \a completionHandler to be executed directly in \a QnStoppableAsync::pleaseStop
    */
    virtual void pleaseStop( std::function<void()>&& completionHandler ) = 0;
};

#endif  //QNSTOPPABLE_H
