/**********************************************************
* 28 jan 2013
* a.kolesnikov
***********************************************************/

#ifndef QNSTOPPABLE_H
#define QNSTOPPABLE_H

#include <functional>
#include <vector>

/** Abstract class providing interface to stop doing anything without object destruction */
class QN_EXPORT QnStoppable
{
public:
    virtual ~QnStoppable() = default;

    virtual void pleaseStop() = 0;
};

#endif  //QNSTOPPABLE_H
