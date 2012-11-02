#include "client_meta_types.h"

#include <common/common_meta_types.h>

#include <camera/thumbnail.h>

#include <ui/workbench/workbench_state.h>

#include "client_globals.h"

namespace {
    volatile bool qn_clientMetaTypes_initialized = false;

    QN_DEFINE_ENUM_STREAM_OPERATORS(Qn::TimeMode);
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
    qRegisterMetaType<QnWorkbenchState>();
    qRegisterMetaTypeStreamOperators<QnWorkbenchState>();
    qRegisterMetaType<QnWorkbenchStateHash>();
    qRegisterMetaTypeStreamOperators<QnWorkbenchStateHash>();
    qRegisterMetaType<Qn::TimeMode>();
    qRegisterMetaTypeStreamOperators<Qn::TimeMode>();

    qn_clientMetaTypes_initialized = true;
}
