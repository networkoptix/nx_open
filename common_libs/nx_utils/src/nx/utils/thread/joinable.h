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
    virtual void join() = 0;
};

#endif  //QNJOINABLE_H
