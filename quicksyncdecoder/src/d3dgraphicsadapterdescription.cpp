////////////////////////////////////////////////////////////
// 1 oct 2012    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#include "d3dgraphicsadapterdescription.h"

#include <DxErr.h>
#include <Windows.h>

#include <plugins/videodecoders/videodecoderplugintypes.h>
#include <utils/common/log.h>


D3DGraphicsAdapterDescription::D3DGraphicsAdapterDescription( IDirect3D9Ex* direct3D9, unsigned int graphicsAdapterNumber )
:
    m_graphicsAdapterNumber( graphicsAdapterNumber )
{
    memset( &m_adapterIdentifier, 0, sizeof(m_adapterIdentifier) );
    HRESULT result = direct3D9->GetAdapterIdentifier(
        graphicsAdapterNumber,
        0,
        &m_adapterIdentifier );
    if( result != S_OK )
    {
        cl_log.log( QString::fromAscii("Failed to read graphics adapter %1 info. %2").
            arg(graphicsAdapterNumber).arg(QString::fromWCharArray(DXGetErrorDescription(result))), cl_logERROR );
        return;
    }

    m_driverVersionStr = QString("%1.%2.%3.%4").
        arg(HIWORD(m_adapterIdentifier.DriverVersion.HighPart)). //Product
        arg(LOWORD(m_adapterIdentifier.DriverVersion.HighPart)).  //Version
        arg(HIWORD(m_adapterIdentifier.DriverVersion.LowPart)).   //SubVersion
        arg(LOWORD(m_adapterIdentifier.DriverVersion.LowPart));   //Build
}

bool D3DGraphicsAdapterDescription::get( int resID, QVariant* const value ) const
{
    switch( resID )
    {
        case DecoderParameter::architecture:
#ifdef _WIN64
            *value = QVariant( "x64" );
#else
            *value = QVariant( "x86" );
#endif
            return true;

        case DecoderParameter::gpuDeviceString:
            *value = QString::fromAscii( m_adapterIdentifier.Description );
            return true;

        case DecoderParameter::driverVersion:
            *value = m_driverVersionStr;
            return true;

        case DecoderParameter::gpuVendorId:
            *value = (unsigned int)m_adapterIdentifier.VendorId;
            return true;

        case DecoderParameter::gpuDeviceId:
            *value = (unsigned int)m_adapterIdentifier.DeviceId;
            return true;

        case DecoderParameter::gpuRevision:
            *value = (unsigned int)m_adapterIdentifier.Revision;
            return true;

        default:
            return false;
    }
}
