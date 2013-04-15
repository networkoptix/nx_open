/**********************************************************
* 12 apr 2013
* akolesnikov
***********************************************************/

#include "common_ref_manager.h"


CommonRefManager::CommonRefManager()
:
    m_refCount( 1 )
{
}

CommonRefManager::~CommonRefManager()
{
}

//!Implementaion of nxpl::NXPluginInterface::addRef
unsigned int CommonRefManager::addRef()
{
    return m_refCount.fetchAndAddOrdered(1) + 1;
}

//!Implementaion of nxpl::NXPluginInterface::addRef
unsigned int CommonRefManager::releaseRef()
{
    unsigned int newRefCounter = m_refCount.fetchAndAddOrdered(-1) - 1;
    if( newRefCounter == 0 )
        delete this;
    return newRefCounter;
}
