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
            case gpuDeviceString:
                return "gpuDeviceString";
            case driverVersion:
                return "driverVersion";
            case gpuVendorId:
                return "gpuVendorId";
            case gpuDeviceId:
                return "gpuDeviceId";
            case gpuRevision:
                return "gpuRevision";
            case sdkVersion:
                return "sdkVersion";
            case architecture:
                return "architecture";
            case framePictureWidth:
                return "framePictureWidth";
            case framePictureHeight:
                return "framePictureHeight";
            case framePictureSize:
                return "framePictureSize";
            case fps:
                return "fps";
            case pixelsPerSecond:
                return "pixelsPerSecond";
            case videoMemoryUsage:
                return "videoMemoryUsage";
            case simultaneousStreamCount:
                return "simultaneousStreamCount";
            default:
                return "unknown";
        }
    }

    Value fromString( const std::string& name )
    {
        if( name == "gpuDeviceString" )
            return gpuDeviceString;
        else if( name == "driverVersion" )
            return driverVersion;
        else if( name == "gpuVendorId" )
            return gpuVendorId;
        else if( name == "gpuDeviceId" )
            return gpuDeviceId;
        else if( name == "gpuRevision" )
            return gpuRevision;
        else if( name == "sdkVersion" )
            return sdkVersion;
        else if( name == "architecture" )
            return architecture;
        else if( name == "framePictureWidth" )
            return framePictureWidth;
        else if( name == "framePictureHeight" )
            return framePictureHeight;
        else if( name == "framePictureSize" )
            return framePictureSize;
        else if( name == "fps" )
            return fps;
        else if( name == "pixelsPerSecond" )
            return pixelsPerSecond;
        else if( name == "videoMemoryUsage" )
            return videoMemoryUsage;
        else if( name == "simultaneousStreamCount" )
            return simultaneousStreamCount;

        return (Value)-1;
    }
}

DecoderResourcesNameset::DecoderResourcesNameset()
{
    registerResource( DecoderParameter::gpuDeviceString, QString::fromAscii("gpuDeviceString"), QVariant::String );
    registerResource( DecoderParameter::driverVersion, QString::fromAscii("driverVersion"), QVariant::String );
    registerResource( DecoderParameter::gpuVendorId, QString::fromAscii("gpuVendorId"), QVariant::UInt );
    registerResource( DecoderParameter::gpuDeviceId, QString::fromAscii("gpuDeviceId"), QVariant::UInt );
    registerResource( DecoderParameter::gpuRevision, QString::fromAscii("gpuRevision"), QVariant::UInt );
    registerResource( DecoderParameter::sdkVersion, QString::fromAscii("sdkVersion"), QVariant::String );
    registerResource( DecoderParameter::architecture, QString::fromAscii("architecture"), QVariant::String );
    registerResource( DecoderParameter::framePictureWidth, QString::fromAscii("framePictureWidth"), QVariant::Int );
    registerResource( DecoderParameter::framePictureHeight, QString::fromAscii("framePictureHeight"), QVariant::Int );
    registerResource( DecoderParameter::framePictureSize, QString::fromAscii("framePictureSize"), QVariant::UInt );
    registerResource( DecoderParameter::fps, QString::fromAscii("fps"), QVariant::Double );
    registerResource( DecoderParameter::pixelsPerSecond, QString::fromAscii("pixelsPerSecond"), QVariant::ULongLong );
    registerResource( DecoderParameter::videoMemoryUsage, QString::fromAscii("videoMemoryUsage"), QVariant::ULongLong );
    registerResource( DecoderParameter::simultaneousStreamCount, QString::fromAscii("simultaneousStreamCount"), QVariant::Int );
}

MediaStreamParameterSumContainer::MediaStreamParameterSumContainer(
    const stree::ResourceNameSet& rns,
    const stree::AbstractResourceReader& rc1,
    const stree::AbstractResourceReader& rc2 )
:
    m_rns( rns ),
    m_rc1( rc1 ),
    m_rc2( rc2 )
{
}

bool MediaStreamParameterSumContainer::get( int resID, QVariant* const value ) const
{
    const QVariant::Type resType = m_rns.findResourceByID( resID ).type;
    if( resType == QVariant::Invalid )
        return false;

    const bool rc1LookupResult = m_rc1.get( resID, value );
    switch( resType )
    {
        case QVariant::Int:
        case QVariant::UInt:
        case QVariant::ULongLong:
        {
            if( !rc1LookupResult )
                return m_rc2.get( resID, value );
            QVariant val2;
            const bool rc2LookupResult = m_rc2.get( resID, &val2 );
            if( !rc2LookupResult )
                return rc1LookupResult;
            //sum
            value->convert( resType );
            val2.convert( resType );
            switch( resType )
            {
                case QVariant::Int:
                    *value = QVariant( value->value<int>() + val2.value<int>() );
                    break;
                case QVariant::UInt:
                    *value = QVariant( value->value<uint>() + val2.value<uint>() );
                    break;
                case QVariant::ULongLong:
                    *value = QVariant( value->value<qulonglong>() + val2.value<qulonglong>() );
                    break;
                default:
                    Q_ASSERT( false );
            }
            return true;
        }

        default:
            if( rc1LookupResult )
                return true;
            return m_rc2.get( resID, value );
    }
}
