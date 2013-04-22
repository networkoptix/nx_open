/**********************************************************
* 12 apr 2013
* akolesnikov
***********************************************************/

#include "common_ref_manager.h"


CommonRefManager::CommonRefManager()
:
    m_refCount( 1 ),
    m_refCountingDelegate( 0 )
{
}

CommonRefManager::CommonRefManager( CommonRefManager* refCountingDelegate )
:
    m_refCountingDelegate( refCountingDelegate )
{
}

CommonRefManager::~CommonRefManager()
{
}

//!Implementaion of nxpl::NXPluginInterface::addRef
unsigned int CommonRefManager::addRef()
{
    return m_refCountingDelegate
        ? m_refCountingDelegate->addRef()
        : m_refCount.fetchAndAddOrdered(1) + 1;
}

//!Implementaion of nxpl::NXPluginInterface::addRef
unsigned int CommonRefManager::releaseRef()
{
    if( m_refCountingDelegate )
        return m_refCountingDelegate->releaseRef();

    unsigned int newRefCounter = m_refCount.fetchAndAddOrdered(-1) - 1;
    if( newRefCounter == 0 )
        delete this;
    return newRefCounter;
}
