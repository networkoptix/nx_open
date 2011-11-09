#ifndef cl_base_h_1012
#define cl_base_h_1012

#define CL_MEDIA_ALIGNMENT 32
#define CL_MEDIA_EXTRA 8

#define QN_DECLARE_PRIVATE(Class) \
class Class##Private; \
    Class##Private* d_ptr; \
    Q_DECLARE_PRIVATE(Class);

#define QN_DECLARE_PRIVATE_DERIVED(Class) \
class Class##Private; \
    Q_DECLARE_PRIVATE(Class);

typedef QPair<QString, QString> QnRequestParam;
typedef QList<QnRequestParam> QnRequestParamList;

#endif //cl_common_h1003
