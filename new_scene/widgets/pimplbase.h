#ifndef PIMPLBASE_H
#define PIMPLBASE_H

#include <QtGlobal> /* For Q_DECLARE_PRIVATE. */
#include <QScopedPointer>

class PimplBasePrivate;

/**
 * Base class for pimpl classes.
 *
 * Was introduced because we cannot use Qt private headers.
 */
class PimplBase {
public:
    virtual ~PimplBase();

protected:
    PimplBase(PimplBasePrivate *dd);

    const QScopedPointer<PimplBasePrivate> d_ptr;

private:
    Q_DECLARE_PRIVATE(PimplBase);
};


#endif // PIMPLBASE_H 
