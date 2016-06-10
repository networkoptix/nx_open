/**********************************************************
* Dec 21, 2015
* akolesnikov
***********************************************************/

#ifndef NX_MEDIATOR_PROCESS_PUBLIC_H
#define NX_MEDIATOR_PROCESS_PUBLIC_H

#include <nx/utils/move_only_func.h>
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

    void setOnStartedEventHandler(
        nx::utils::MoveOnlyFunc<void(bool /*result*/)> handler);

    virtual void pleaseStop() override;

    int exec();

    const MediatorProcess* impl() const;

private:
    MediatorProcess* m_impl;
};

}   //hpm
}   //nx

#endif  //NX_MEDIATOR_PROCESS_PUBLIC_H
