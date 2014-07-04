/**********************************************************
* 27 jun 2014
* a.kolesnikov
***********************************************************/

#ifndef NX_SYSTEM_ALLOCATOR_H
#define NX_SYSTEM_ALLOCATOR_H

#include "abstract_allocator.h"


//!Allocates memory using \a malloc and frees with \a free
class QnSystemAllocator
:
    public QnAbstractAllocator
{
public:
    virtual void* alloc( size_t size ) override;
    virtual void release( void* ptr ) override;

    static QnSystemAllocator* instance();
};

#endif //NX_SYSTEM_ALLOCATOR_H
