// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>

#include <nx/streaming/video_data_packet.h>
#include <nx/streaming/media_data_packet.h>
#include <core/resource/avi/avi_archive_metadata.h>

namespace nx::recording::helpers {

NX_VMS_COMMON_API bool addStream(const CodecParametersConstPtr& codecParameters, AVFormatContext* formatContext);
NX_VMS_COMMON_API QByteArray serializeMetadataPacket(const QnConstAbstractCompressedMetadataPtr& data);
NX_VMS_COMMON_API QnAbstractCompressedMetadataPtr deserializeMetaDataPacket(const QByteArray& data);

} // namespace nx::recording::helpers
