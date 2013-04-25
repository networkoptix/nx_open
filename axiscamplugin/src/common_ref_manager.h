/**********************************************************
* 12 apr 2013
* akolesnikov
***********************************************************/

#ifndef COMMON_REF_MANAGER_H
#define COMMON_REF_MANAGER_H

#include <QAtomicInt>


//!Implements \a nxpl::NXPluginInterface reference counting. Can delegate reference counting to another \a CommonRefManager instance
/*!
    This class does not inherit nxpl::NXPluginInterface because it would require virtual inheritance
*/
class CommonRefManager
{
public:
    //!Use this constructor to store reference counter in constructed object
    CommonRefManager();
    //!Use this constructor to delegate reference counting to another object
    CommonRefManager( CommonRefManager* refCountingDelegate );
    virtual ~CommonRefManager();

    //!Implementaion of nxpl::NXPluginInterface::addRef
    unsigned int addRef();
    //!Implementaion of nxpl::NXPluginInterface::releaseRef
    /*!
        Calls delete this if reference counter reached zero
    */
    unsigned int releaseRef();

private:
    QAtomicInt m_refCount;
    CommonRefManager* m_refCountingDelegate;
};

#endif  //COMMON_REF_MANAGER_H
