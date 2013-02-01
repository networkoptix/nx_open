/**********************************************************
* 28 jan 2013
* a.kolesnikov
***********************************************************/

#ifndef QNSTOPPABLE_H
#define QNSTOPPABLE_H


//!Abstract class providing interface to stop doing anything without object destruction
class QN_EXPORT QnStoppable
{
public:
    virtual ~QnStoppable() {}

    virtual void pleaseStop() = 0;
};

#endif  //QNSTOPPABLE_H
