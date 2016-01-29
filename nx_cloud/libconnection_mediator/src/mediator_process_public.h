/**********************************************************
* Dec 21, 2015
* akolesnikov
***********************************************************/

#ifndef NX_MEDIATOR_PROCESS_PUBLIC_H
#define NX_MEDIATOR_PROCESS_PUBLIC_H

#include <utils/common/stoppable.h>


namespace nx {
namespace hpm {

class MediatorProcess;

class MediatorProcessPublic
:
    public QnStoppable
{
public:
    MediatorProcessPublic(int argc, char **argv);
    virtual ~MediatorProcessPublic();

    virtual void pleaseStop() override;

    int exec();

private:
    MediatorProcess* m_impl;
};

}   //hpm
}   //nx

#endif  //NX_MEDIATOR_PROCESS_PUBLIC_H
