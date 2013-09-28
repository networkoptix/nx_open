/**********************************************************
* 27 sep 2013
* a.kolesnikov
***********************************************************/

#ifndef QNJOINABLE_H
#define QNJOINABLE_H


//!Interface to wait for instance finishes its tasks before destruction
class QN_EXPORT QnJoinable
{
public:
    virtual ~QnJoinable() {}

    //!Blocks till instance does what it should before destruction
    /*!
        \todo Name \a wait has been chosen only for compatibility with \a QnLongRunnable. Correct name for this is \a join()
    */
    virtual void wait() = 0;
};

#endif  //QNJOINABLE_H
