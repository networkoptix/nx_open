#ifndef CONFIGURE_REPLY_H
#define CONFIGURE_REPLY_H

#include <QtCore/QMetaType>

#include <nx/fusion/model_functions_fwd.h>

struct QnConfigureReply {
    QnConfigureReply() : restartNeeded(false) {}

    bool restartNeeded;
};

#define QnConfigureReply_Fields (restartNeeded)

QN_FUSION_DECLARE_FUNCTIONS(QnConfigureReply, (json)(metatype))

#endif // CONFIGURE_REPLY_H
