#ifndef QN_BASE_H
#define QN_BASE_H

// TODO: these defines do not belong here
#define CL_MEDIA_ALIGNMENT 32
#define CL_MEDIA_EXTRA 8

#define QN_DECLARE_PRIVATE(Class) \
class Class##Private; \
    Class##Private* d_ptr; \
    Q_DECLARE_PRIVATE(Class);

#define QN_DECLARE_PRIVATE_DERIVED(Class) \
class Class##Private; \
    Q_DECLARE_PRIVATE(Class);

#endif //cl_common_h1003
