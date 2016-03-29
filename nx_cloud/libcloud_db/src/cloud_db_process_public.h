/**********************************************************
* Nov 25, 2015
* akolesnikov
***********************************************************/

#ifndef NX_CLOUD_DB_PROCESS_PUBLIC_H
#define NX_CLOUD_DB_PROCESS_PUBLIC_H

#include <utils/common/stoppable.h>


namespace nx {
namespace cdb {

class CloudDBProcess;

class CloudDBProcessPublic
:
    public QnStoppable
{
public:
    CloudDBProcessPublic(int argc, char **argv);
    virtual ~CloudDBProcessPublic();

    virtual void pleaseStop() override;

    void setOnStartedEventHandler(
        nx::utils::MoveOnlyFunc<void(bool /*result*/)> handler);

    int exec();

private:
    CloudDBProcess* m_impl;
};

}   //cdb
}   //nx

#endif  //NX_CLOUD_DB_PROCESS_PUBLIC_H
