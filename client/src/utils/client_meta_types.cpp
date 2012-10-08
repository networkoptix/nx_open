#include "client_meta_types.h"

#include <common/common_meta_types.h>

#include <camera/thumbnail.h>

#include <ui/workbench/workbench_globals.h>

namespace {
    volatile bool qn_clientMetaTypes_initialized = false;
}

void QnClientMetaTypes::initialize() {
    /* Note that running the code twice is perfectly OK, 
     * so we don't need heavyweight synchronization here. */
    if(qn_clientMetaTypes_initialized)
        return;

    QnCommonMetaTypes::initilize();

    qRegisterMetaType<Qn::ItemRole>();
    qRegisterMetaType<QnThumbnail>();
    qRegisterMetaType<QVector<QUuid> >();

    qn_clientMetaTypes_initialized = true;
}
