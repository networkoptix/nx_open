#include "server_meta_types.h"

#include <providers/stream_mixer.h>

void QnServerMetaTypes::initialize()
{
    qRegisterMetaType<QnChannelMapping>();
    qRegisterMetaType<QList<QnChannelMapping>>();
    qRegisterMetaType<QnResourceChannelMapping>();
    qRegisterMetaType<QList<QnResourceChannelMapping>>();

    QnJsonSerializer::registerSerializer<QList<QnChannelMapping>>();
    QnJsonSerializer::registerSerializer<QList<QnResourceChannelMapping>>();
}
