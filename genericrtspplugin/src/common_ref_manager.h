/**********************************************************
* 12 apr 2013
* akolesnikov
***********************************************************/

#ifndef COMMON_REF_MANAGER_H
#define COMMON_REF_MANAGER_H

#include <QAtomicInt>

#include <plugins/nx_plugin_api.h>


//!Implements \a nxpl::NXPluginInterface reference counting. Can delegate reference counting to another \a CommonRefManager instance
class CommonRefManager
:
    virtual public nxpl::NXPluginInterface
{
public:
    //!Use this constructor to store reference counter in constructed object
    CommonRefManager();
    //!Use this constructor to delegate reference counting to another object
    CommonRefManager( CommonRefManager* refCountingDelegate );
    virtual ~CommonRefManager();

    //!Implementaion of nxpl::NXPluginInterface::addRef
    virtual unsigned int addRef() override;
    //!Implementaion of nxpl::NXPluginInterface::addRef
    virtual unsigned int releaseRef() override;

private:
    QAtomicInt m_refCount;
    CommonRefManager* m_refCountingDelegate;
};

#endif  //COMMON_REF_MANAGER_H
