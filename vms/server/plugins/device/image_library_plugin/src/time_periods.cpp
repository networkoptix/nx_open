// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "time_periods.h"

TimePeriods::TimePeriods()
:
    m_refManager( this )
{
}

//!Implementation of nxpl::PluginInterface::queryInterface
void* TimePeriods::queryInterface( const nxpl::NX_GUID& interfaceID )
{
    if( memcmp( &interfaceID, &nxcip::IID_TimePeriods, sizeof(nxcip::IID_TimePeriods) ) == 0 )
    {
        addRef();
        return static_cast<nxcip::TimePeriods*>(this);
    }
    return NULL;
}

//!Implementation of nxpl::PluginInterface::addRef
int TimePeriods::addRef() const
{
    return m_refManager.addRef();
}

//!Implementation of nxpl::PluginInterface::releaseRef
int TimePeriods::releaseRef() const
{
    return m_refManager.releaseRef();
}

//!Implementation of nxcip::TimePeriods::get
bool TimePeriods::get( nxcip::UsecUTCTimestamp* start, nxcip::UsecUTCTimestamp* end ) const
{
    if( m_pos == timePeriods.end() )
        return false;
    *start = m_pos->first;
    *end = m_pos->second;
    return true;
}

void TimePeriods::goToBeginning()
{
    m_pos = timePeriods.begin();
}

bool TimePeriods::next()
{
    return (++m_pos) != timePeriods.end();
}

bool TimePeriods::atEnd() const
{
    return m_pos == timePeriods.end();
}
