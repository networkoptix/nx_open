////////////////////////////////////////////////////////////
// 25 sep 2012    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#ifndef VIDEODECODERPLUGINTYPES_H
#define VIDEODECODERPLUGINTYPES_H

#include <string>

#include <plugins/videodecoders/stree/resourcecontainer.h>
#include <plugins/videodecoders/stree/resourcenameset.h>


namespace DecoderParameter
{
    enum Value
    {
        //string
        decoderName = 1,
        //string
        osName,
        //string. CPU archtecture (x64 or x86)
        architecture,
        //string
        gpuDeviceString,
        //string. e.g. 1.2.3.4 (Product.Version.SubVersion.Build)
        driverVersion,
        //unsigned int
        gpuVendorId,
        //unsigned int
        gpuDeviceId,
        //unsigned int
        gpuRevision,
        //string
        sdkVersion,
        //int. Width of decoded frame picture in pixels (before any post-processing)
        framePictureWidth,
        //int. Height of decoded frame picture in pixels (before any post-processing)
        framePictureHeight,
        //unsigned int. Decoded picture size in pixels (product of \a framePictureWidth and \a framePictureHeight)
        framePictureSize,
        //double
        fps,
        //uint64
        pixelsPerSecond,
        //uint64
        videoMemoryUsage,
        //int
        simultaneousStreamCount
    };

    std::string toString( const Value& val );
    Value fromString( const std::string& name );
}

class DecoderResourcesNameset
:
    public stree::ResourceNameSet
{
public:
    DecoderResourcesNameset();
};

//!Parameters of running decoding session
class DecoderStreamDescription
:
    public stree::ResourceContainer
{
public:
};

//!Parameter of media stream
/*!
    Summarizes integer and float resource values from both containers (overflow is ignored), resources of other types are searched in first container, 
    than, if not found, searched for in a second container
*/
class MediaStreamParameterSumContainer
:
    public stree::AbstractResourceReader
{
public:
    MediaStreamParameterSumContainer(
        const stree::ResourceNameSet& rns,
        const stree::AbstractResourceReader& rc1,
        const stree::AbstractResourceReader& rc2 );

    virtual bool get( int resID, QVariant* const value ) const;

private:
    const stree::ResourceNameSet& m_rns;
    const stree::AbstractResourceReader& m_rc1;
    const stree::AbstractResourceReader& m_rc2;
};

#endif  //VIDEODECODERPLUGINTYPES_H
