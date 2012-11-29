////////////////////////////////////////////////////////////
// 1 oct 2012    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#include "linuxgraphicsadapterdescription.h"

#include <plugins/videodecoders/videodecoderplugintypes.h>
#include <utils/common/log.h>

#include <GL/glx.h>


LinuxGraphicsAdapterDescription::LinuxGraphicsAdapterDescription( unsigned int graphicsAdapterNumber )
:
    m_graphicsAdapterNumber( graphicsAdapterNumber )
{
    m_gpuDeviceString = QString::fromAscii( reinterpret_cast<const char *>(glGetString(GL_RENDERER)) );
    QByteArray vendor = reinterpret_cast<const char *>(glGetString(GL_VENDOR));


//    memset( &m_adapterIdentifier, 0, sizeof(m_adapterIdentifier) );
//    HRESULT result = direct3D9->GetAdapterIdentifier(
//        graphicsAdapterNumber,
//        0,
//        &m_adapterIdentifier );
//    if( result != S_OK )
//    {
//        cl_log.log( QString::fromAscii("Failed to read graphics adapter %1 info. %2").
//            arg(graphicsAdapterNumber).arg(QString::fromWCharArray(DXGetErrorDescription(result))), cl_logERROR );
//        return;
//    }
//
//    m_driverVersionStr = QString("%1.%2.%3.%4").
//        arg(HIWORD(m_adapterIdentifier.DriverVersion.HighPart)). //Product
//        arg(LOWORD(m_adapterIdentifier.DriverVersion.HighPart)).  //Version
//        arg(HIWORD(m_adapterIdentifier.DriverVersion.LowPart)).   //SubVersion
//        arg(LOWORD(m_adapterIdentifier.DriverVersion.LowPart));   //Build
}

bool LinuxGraphicsAdapterDescription::get( int resID, QVariant* const value ) const
{
    switch( resID )
    {
        case DecoderParameter::architecture:
#ifdef __LP64__
            *value = QVariant( "x64" );
#else
            *value = QVariant( "x86" );
#endif
            return true;

        case DecoderParameter::gpuDeviceString:
            *value = m_gpuDeviceString;
            return true;

        case DecoderParameter::driverVersion:
            *value = m_driverVersionStr;
            return true;

        case DecoderParameter::gpuVendorId:
//            *value = (unsigned int)m_adapterIdentifier.VendorId;
            return true;

        case DecoderParameter::gpuDeviceId:
//            *value = (unsigned int)m_adapterIdentifier.DeviceId;
            return true;

        case DecoderParameter::gpuRevision:
//            *value = (unsigned int)m_adapterIdentifier.Revision;
            return true;

        default:
            return false;
    }
}
