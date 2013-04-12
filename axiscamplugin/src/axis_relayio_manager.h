/**********************************************************
* 12 apr 2013
* akolesnikov
***********************************************************/

#ifndef AXIS_RELAYIO_MANAGER_H
#define AXIS_RELAYIO_MANAGER_H

#include <plugins/camera_plugin.h>


class AxisRelayIOManager
:
    public nxcip::CameraRelayIOManager
{
public:
    AxisRelayIOManager();
    virtual ~AxisRelayIOManager();

    //!Implementation of nxcip::CameraRelayIOManager::getRelayOutputList
    virtual int getRelayOutputList( char** idList, int* idNum ) const override;
    //!Implementation of nxcip::CameraRelayIOManager::getRelayOutputList
    virtual int getInputPortList( char** idList, int* idNum ) const override;
    //!Implementation of nxcip::CameraRelayIOManager::getRelayOutputList
    virtual int setRelayOutputState(
        const char* ouputID,
        bool activate,
        unsigned int autoResetTimeoutMS ) override;
    //!Implementation of nxcip::CameraRelayIOManager::getRelayOutputList
    virtual int startInputPortMonitoring() override;
    //!Implementation of nxcip::CameraRelayIOManager::getRelayOutputList
    virtual void stopInputPortMonitoring() override;
    //!Implementation of nxcip::CameraRelayIOManager::getRelayOutputList
    virtual void registerEventHandler( nxcip::CameraInputEventHandler* handler ) override;
    //!Implementation of nxcip::CameraRelayIOManager::getRelayOutputList
    virtual void unregisterEventHandler( nxcip::CameraInputEventHandler* handler ) override;
    //!Implementation of nxcip::CameraRelayIOManager::getRelayOutputList
    virtual void getLastErrorString( char* errorString ) const override;
};

#endif  //AXIS_RELAYIO_MANAGER_H
