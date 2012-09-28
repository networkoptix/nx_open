////////////////////////////////////////////////////////////
// 25 sep 2012    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#include "videodecoderplugintypes.h"


namespace DecoderParameter
{
    std::string toString( const Value& val )
    {
        switch( val )
        {
            case gpuVendorName:
                return "gpuVendorName";
            case gpuName:
                return "gpuName";
            case gpuRevision:
                return "gpuRevision";
            case framePictureSize:
                return "framePictureSize";
            case fps:
                return "fps";
            case pixelsPerSecond:
                return "pixelsPerSecond";
            case videoMemoryUsage:
                return "videoMemoryUsage";
            default:
                return "unknown";
        }
    }

    Value fromString( const std::string& name )
    {
        if( name == "gpuVendorName" )
            return gpuVendorName;
        else if( name == "gpuVendorName" )
            return gpuVendorName;
        else if( name == "gpuName" )
            return gpuName;
        else if( name == "gpuRevision" )
            return gpuRevision;
        else if( name == "framePictureSize" )
            return framePictureSize;
        else if( name == "fps" )
            return fps;
        else if( name == "pixelsPerSecond" )
            return pixelsPerSecond;
        else if( name == "videoMemoryUsage" )
            return videoMemoryUsage;

        return (Value)-1;
    }
}
