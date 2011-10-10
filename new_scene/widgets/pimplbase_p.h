#ifndef PIMPLBASE_P_H
#define PIMPLBASE_P_H

#include <QtGlobal> /* For Q_DECLARE_PUBLIC. */
#include "PimplBase.h"

/**
 * Base class for private pimpl classes.
 */
class PimplBasePrivate {
public:
    PimplBasePrivate(): q_ptr(NULL) {}

    virtual ~PimplBasePrivate() {}

protected:
    PimplBase *const q_ptr;

private:
    Q_DECLARE_PUBLIC(PimplBase);
};

#endif // PIMPLBASE_P_H 
