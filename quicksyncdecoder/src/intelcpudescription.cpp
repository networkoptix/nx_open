////////////////////////////////////////////////////////////
// 31 jan 2013    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#include "intelcpudescription.h"

#include <intrin.h>

#include <plugins/videodecoders/videodecoderplugintypes.h>


IntelCPUDescription::IntelCPUDescription()
:
    m_cpuFamily( 0 ),
    m_cpuModel( 0 ),
    m_cpuStepping( 0 )
{
    readCPUInfo();
}

bool IntelCPUDescription::get( int resID, QVariant* const value ) const
{
    switch( resID )
    {
        case DecoderParameter::cpuString:
            *value = m_cpuString;
            return true;
        case DecoderParameter::cpuFamily:
            *value = m_cpuFamily;
            return true;
        case DecoderParameter::cpuModel:
            *value = m_cpuModel;
            return true;
        case DecoderParameter::cpuStepping:
            *value = m_cpuStepping;
            return true;
        default:
            return false;
    }
}

void IntelCPUDescription::readCPUInfo()
{
    char CPUBrandString[0x40];
    int CPUInfo[4] = {-1};

    __cpuid(CPUInfo, 1);
    m_cpuFamily = (((CPUInfo[0] >> 20) & 0xff) << 4) | (CPUInfo[0] >> 8) & 0x0f;
    m_cpuModel = (((CPUInfo[0] >> 16) & 0x0f) << 4) | ((CPUInfo[0] >> 4) & 0x0f);
    m_cpuStepping = CPUInfo[0] & 0x0f;

    // Calling __cpuid with 0x80000000 as the InfoType argument
    // gets the number of valid extended IDs.
    __cpuid(CPUInfo, 0x80000000);
    unsigned int nExIds = CPUInfo[0];
    memset(CPUBrandString, 0, sizeof(CPUBrandString));

    // Get the information associated with each extended ID.
    for( unsigned int i=0x80000000; i<=nExIds; ++i )
    {
        __cpuid(CPUInfo, i);

        // Interpret CPU brand string and cache information.
        if  (i == 0x80000002)
            memcpy(CPUBrandString, CPUInfo, sizeof(CPUInfo));
        else if  (i == 0x80000003)
            memcpy(CPUBrandString + 16, CPUInfo, sizeof(CPUInfo));
        else if  (i == 0x80000004)
            memcpy(CPUBrandString + 32, CPUInfo, sizeof(CPUInfo));
    }

    if( nExIds >= 0x80000004 )
    {
        //printf_s("\nCPU Brand String: %s\n", CPUBrandString);
        m_cpuString = QString::fromAscii( CPUBrandString ).trimmed();
    }
}
