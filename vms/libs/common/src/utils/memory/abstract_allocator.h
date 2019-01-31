/**********************************************************
* 27 jun 2014
* a.kolesnikov
***********************************************************/

#ifndef NX_ABSTRACT_ALLOCATOR_H
#define NX_ABSTRACT_ALLOCATOR_H

#include <stdlib.h>


/*!
    Using abstract class for allocator to leave QnAbstractMediaData and children largely unchanged
*/
class QnAbstractAllocator
{
public:
    virtual ~QnAbstractAllocator() {}

    virtual void* alloc( size_t size ) = 0;
    virtual void release( void* ptr ) = 0;
};

#endif  //NX_ABSTRACT_ALLOCATOR_H
