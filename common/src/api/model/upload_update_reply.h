#ifndef UPLOAD_UPDATE_REPLY_H
#define UPLOAD_UPDATE_REPLY_H

#include <QtCore/QMetaType>

#include <utils/common/model_functions_fwd.h>

struct QnUploadUpdateReply {
    QnUploadUpdateReply() : offset(0) {}

    qint64 offset;
};

#define QnUploadUpdateReply_Fields (offset)

QN_FUSION_DECLARE_FUNCTIONS(QnUploadUpdateReply, (json)(metatype))

#endif // UPLOAD_UPDATE_REPLY_H

