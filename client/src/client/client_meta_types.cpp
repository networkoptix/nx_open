#include "client_meta_types.h"

#include <common/common_meta_types.h>

#include <camera/thumbnail.h>

#include "client_globals.h"
#include "client_model_types.h"
#include "utils/color_space/image_correction.h"

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

    qRegisterMetaType<QVector<QUuid> >();
    qRegisterMetaType<QVector<QColor> >();

    qRegisterMetaType<Qn::ItemRole>();
    qRegisterMetaType<QnThumbnail>();    
    qRegisterMetaType<QnWorkbenchState>();
    qRegisterMetaTypeStreamOperators<QnWorkbenchState>();
    qRegisterMetaType<QnWorkbenchStateHash>();
    qRegisterMetaTypeStreamOperators<QnWorkbenchStateHash>();
    qRegisterMetaType<QnLicenseWarningState>();
    qRegisterMetaTypeStreamOperators<QnLicenseWarningState>();
    qRegisterMetaType<QnLicenseWarningStateHash>();
    qRegisterMetaTypeStreamOperators<QnLicenseWarningStateHash>();
    qRegisterMetaType<QnServerStorageKey>();
    qRegisterMetaTypeStreamOperators<QnServerStorageKey>();
    qRegisterMetaType<QnServerStorageStateHash>();
    qRegisterMetaTypeStreamOperators<QnServerStorageStateHash>();
    qRegisterMetaType<Qn::TimeMode>();
    qRegisterMetaTypeStreamOperators<Qn::TimeMode>();
    qRegisterMetaType<ImageCorrectionParams>();

    qn_clientMetaTypes_initialized = true;
}
