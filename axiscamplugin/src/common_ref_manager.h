/**********************************************************
* 12 apr 2013
* akolesnikov
***********************************************************/

#ifndef COMMON_REF_MANAGER_H
#define COMMON_REF_MANAGER_H

#include <QAtomicInt>

#include <plugins/plugin_api.h>


//!Implements \a nxpl::PluginInterface reference counting. Can delegate reference counting to another \a CommonRefManager instance
/*!
    This class does not inherit nxpl::PluginInterface because it would require virtual inheritance.
    This class is supposed to be nested to monitored class
*/
class CommonRefManager
{
public:
    //!Use this constructor to delete \a objToWatch when reference counter drops to zero
    CommonRefManager( nxpl::PluginInterface* objToWatch );
    //!Use this constructor to delegate reference counting to another object
    CommonRefManager( CommonRefManager* refCountingDelegate );
    virtual ~CommonRefManager();

    //!Implementaion of nxpl::PluginInterface::addRef
    unsigned int addRef();
    //!Implementaion of nxpl::PluginInterface::releaseRef
    /*!
        Deletes monitored object if reference counter reached zero
    */
    unsigned int releaseRef();

private:
    QAtomicInt m_refCount;
    nxpl::PluginInterface* m_objToWatch;
    CommonRefManager* m_refCountingDelegate;
};

#endif  //COMMON_REF_MANAGER_H
