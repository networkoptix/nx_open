/**********************************************************
* 12 apr 2013
* akolesnikov
***********************************************************/

#include "axis_relayio_manager.h"


AxisRelayIOManager::AxisRelayIOManager()
{
}

AxisRelayIOManager::~AxisRelayIOManager()
{
}

int AxisRelayIOManager::getRelayOutputList( char** idList, int* idNum ) const
{
    //TODO/IMPL
    return nxcip::NX_NOT_IMPLEMENTED;
}

int AxisRelayIOManager::getInputPortList( char** idList, int* idNum ) const
{
    //TODO/IMPL
    return nxcip::NX_NOT_IMPLEMENTED;
}

int AxisRelayIOManager::setRelayOutputState(
    const char* ouputID,
    bool activate,
    unsigned int autoResetTimeoutMS )
{
    //TODO/IMPL
    return nxcip::NX_NOT_IMPLEMENTED;
}

int AxisRelayIOManager::startInputPortMonitoring()
{
    //TODO/IMPL
    return nxcip::NX_NOT_IMPLEMENTED;
}

void AxisRelayIOManager::stopInputPortMonitoring()
{
    //TODO/IMPL
}

void AxisRelayIOManager::registerEventHandler( nxcip::CameraInputEventHandler* handler )
{
    //TODO/IMPL
}

void AxisRelayIOManager::unregisterEventHandler( nxcip::CameraInputEventHandler* handler )
{
    //TODO/IMPL
}

void AxisRelayIOManager::getLastErrorString( char* errorString ) const
{
    //TODO/IMPL
}
