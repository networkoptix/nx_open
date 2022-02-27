// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

/**********************************************************
* 3 sep 2014
* a.kolesnikov
***********************************************************/

#ifndef QN_CAMERA_LIST_REPLY_H
#define QN_CAMERA_LIST_REPLY_H

#include <nx/utils/uuid.h>

#include <nx/fusion/model_functions_fwd.h>


//!mediaserver's response to \a ping request
class QnCameraListReply
{
public:
    QVector<QString> uniqueIdList;
};

#define QnCameraListReply_Fields (uniqueIdList)

QN_FUSION_DECLARE_FUNCTIONS(QnCameraListReply, (json)(ubjson), NX_VMS_COMMON_API)
Q_DECLARE_METATYPE(QnCameraListReply)

#endif  //QN_CAMERA_LIST_REPLY_H
