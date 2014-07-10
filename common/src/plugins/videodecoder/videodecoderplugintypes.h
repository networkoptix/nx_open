////////////////////////////////////////////////////////////
// 25 sep 2012    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#ifndef VIDEODECODERPLUGINTYPES_H
#define VIDEODECODERPLUGINTYPES_H

#include <string>

#include <plugins/videodecoder/stree/resourcecontainer.h>
#include <plugins/videodecoder/stree/resourcenameset.h>


namespace DecoderParameter
{
    // TODO: #Elric #enum
    enum Value
    {
        //string
        decoderName = 1,
        //string
        osName,
        //string. CPU architecture (x64 or x86)
        architecture,
        //string. CPU brand and name
        cpuString,
        //unsigned int. Received with cpuid instruction
        cpuFamily,
        //unsigned int. Received with cpuid instruction
        cpuModel,
        //unsigned int. Received with cpuid instruction
        cpuStepping,
        //string. Device string of graphics adapter, application is using to display
        displayAdapterDeviceString,
        //string. Info of adapter, hardware acceleration is available on
        gpuDeviceString,
        //string. e.g. 1.2.3.4 (Product.Version.SubVersion.Build). Info of adapter, quicksync is available on
        driverVersion,
        //unsigned int. Info of adapter, quicksync is available on
        gpuVendorId,
        //unsigned int. Info of adapter, quicksync is available on
        gpuDeviceId,
        //unsigned int. Info of adapter, quicksync is available on
        gpuRevision,
        //string. Version of Intel MFX SDK available
        sdkVersion,
        //int. Width of decoded frame picture in pixels (before any post-processing)
        framePictureWidth,
        //int. Height of decoded frame picture in pixels (before any post-processing)
        framePictureHeight,
        //unsigned int. Decoded picture size in pixels (product of \a framePictureWidth and \a framePictureHeight)
        framePictureSize,
        //double
        fps,
        //double
        speed,
        //uint64
        pixelsPerSecond,
        //uint64
        videoMemoryUsage,
        //uint64
        availableVideoMemory,
        //int
        /*!
            In input parameters - it is current number of decoders of type we are checking (if we are checking Quicksync plugin, 
                it is a current number of quicksync decoders running).

            In output parameters it is maximum allowed number of decoders
        */
        simultaneousStreamCount,
        //!int. Number of all running decoders (SW and HW)
        totalCurrentNumberOfDecoders,
        //!unsigned int
        graphicAdapterCount
    };

    //std::string toString( const Value& val );
    //Value fromString( const std::string& name );
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
