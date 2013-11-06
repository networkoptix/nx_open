////////////////////////////////////////////////////////////
// 4 feb 2013    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#include "mfxframeinfo_resource_reader.h"

#include <plugins/videodecoders/videodecoderplugintypes.h>


MFXFrameInfoResourceReader::MFXFrameInfoResourceReader( const mfxFrameInfo& frameInfo )
:
    m_frameInfo( frameInfo )
{
}

bool MFXFrameInfoResourceReader::get( int resID, QVariant* const value ) const
{
    switch( resID )
    {
        case DecoderParameter::framePictureWidth:
            *value = m_frameInfo.Width;
            return true;
        case DecoderParameter::framePictureHeight:
            *value = m_frameInfo.Height;
            return true;
        case DecoderParameter::framePictureSize:
            *value = m_frameInfo.Width * m_frameInfo.Height;
            return true;
        case DecoderParameter::fps:
            *value = m_frameInfo.FrameRateExtN / (float)m_frameInfo.FrameRateExtD;
            return true;
        case DecoderParameter::pixelsPerSecond:
            *value = (m_frameInfo.FrameRateExtN / (float)m_frameInfo.FrameRateExtD) * m_frameInfo.Width*m_frameInfo.Height;
            return true;
        default:
            return false;
    }
}
