////////////////////////////////////////////////////////////
// 25 sep 2012    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#ifndef VIDEODECODERPLUGINTYPES_H
#define VIDEODECODERPLUGINTYPES_H

#include <string>

#include <plugins/videodecoders/stree/resourcecontainer.h>


namespace DecoderParameter
{
    enum Value
    {
        gpuVendorName = 1,
        gpuName,
        gpuRevision,
        framePictureSize,
        fps,
        pixelsPerSecond,
        videoMemoryUsage
    };

    std::string toString( const Value& val );
    Value fromString( const std::string& name );
}

//!Parameters of running decoding session
class DecoderStreamDescription
:
    public stree::ResourceContainer
{
public:
};

//!Parameter of media stream
class MediaStreamParameter
{
public:
};

#endif  //VIDEODECODERPLUGINTYPES_H
