// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <camera/camera_plugin.h>
#include <nx/sdk/archive/i_codec_info.h>
#include <nx/sdk/i_list.h>
#include <nx/sdk/ptr.h>

namespace nx {
namespace sdk {
namespace archive {

// FBE3E8A5-E70C-4E1A-BB13-B77C218EF325
static const nxpl::NX_GUID IID_StreamReader2 = { { 0xFB, 0xE3, 0xE8, 0xA5, 0xE7, 0x0C, 0x4E, 0x1A, 0xBB, 0x13, 0xB7, 0x7C, 0x21, 0x8E, 0xF3, 0x25 } };

class StreamReader2: public nxcip::StreamReader
{
    protected: virtual const IList<ICodecInfo>* getCodecList() const = 0;
    public: Ptr<const IList<ICodecInfo>> codecList() const { return toPtr(getCodecList()); }

public:
    virtual bool providesMotionPackets() const = 0;
};

} // namespace archive
} // namespace sdk
} // namespace nx
