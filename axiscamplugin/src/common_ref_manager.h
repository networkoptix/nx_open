/**********************************************************
* 12 apr 2013
* akolesnikov
***********************************************************/

#ifndef COMMON_REF_MANAGER_H
#define COMMON_REF_MANAGER_H

#include <QAtomicInt>

#include <plugins/nx_plugin_api.h>


class CommonRefManager
:
    virtual public nxpl::NXPluginInterface
{
public:
    CommonRefManager();
    virtual ~CommonRefManager();

    //!Implementaion of nxpl::NXPluginInterface::addRef
    virtual unsigned int addRef() override;
    //!Implementaion of nxpl::NXPluginInterface::addRef
    virtual unsigned int releaseRef() override;

private:
    QAtomicInt m_refCount;
};

#endif  //COMMON_REF_MANAGER_H
