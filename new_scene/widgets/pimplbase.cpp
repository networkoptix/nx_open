#include "PimplBase.h"
#include "PimplBase_p.h"
#include <cassert>

PimplBase::PimplBase(PimplBasePrivate *dd): d_ptr(dd) {
    assert(dd != NULL);

    const_cast<PimplBase *&>(dd->q_ptr) = this;
}

PimplBase::~PimplBase() {
    return;
} 
