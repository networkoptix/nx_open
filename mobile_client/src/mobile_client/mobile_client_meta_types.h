#ifndef MOBILE_CLIENT_META_TYPES_H
#define MOBILE_CLIENT_META_TYPES_H

#include <common/common_meta_types.h>

class QnMobileClientMetaTypes {
public:
    static void initialize();

private:
    static void registerQmlTypes();
};

#endif // MOBILE_CLIENT_META_TYPES_H
