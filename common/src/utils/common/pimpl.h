#ifndef QN_PIMPL_H
#define QN_PIMPL_H

#include <QtCore/QtGlobal>

#define QN_DECLARE_PRIVATE(Class)                                               \
class Class##Private;                                                           \
    Class##Private* d_ptr;                                                      \
    Q_DECLARE_PRIVATE(Class);

#define QN_DECLARE_PRIVATE_DERIVED(Class)                                       \
class Class##Private;                                                           \
    Q_DECLARE_PRIVATE(Class);

#endif // QN_PIMPL_H
