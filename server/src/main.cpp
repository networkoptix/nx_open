#include "common/base/log.h"
#include "common/device/device.h"
#include "common/device/device_managmen/device_manager.h"
#include "common/device_plugins/arecontvision/devices/av_device_server.h"
#include "common/device_plugins/fake/devices/fake_device_server.h"
#include "common/device_plugins/avigilon/devices/avigilon_device_server.h"
#include "common/device_plugins/android/devices/android_device_server.h"
#include "common/device_plugins/iqeye/devices/iqeye_device_server.h"

int main(int argc, char *argv[])
{

	if (!cl_log.create("/log/log_file", 1024*1024*10, 5, cl_logDEBUG1))
    {
        return 0;
    }

#ifdef _DEBUG
	 cl_log.setLogLevel(cl_logDEBUG1);
	//cl_log.setLogLevel(cl_logWARNING);
#else
	cl_log.setLogLevel(cl_logWARNING);
#endif

	CL_LOG(cl_logALWAYS)
	{
		cl_log.log("\n\n========================================", cl_logALWAYS);
		cl_log.log(cl_logALWAYS, "Software version %s", 0);
		cl_log.log(argv[0], cl_logALWAYS);
	}



	CLDevice::startCommandProc();

	CLDeviceManager::instance(); // to initialize net state;

	//===========================================================================
	//IPPH264Decoder::dll.init();



	//============================
	CLDeviceManager::instance().getDeviceSearcher().addDeviceServer(&AVDeviceServer::instance());
	CLDeviceManager::instance().getDeviceSearcher().addDeviceServer(&FakeDeviceServer::instance());
    CLDeviceManager::instance().getDeviceSearcher().addDeviceServer(&AVigilonDeviceServer::instance());
    CLDeviceManager::instance().getDeviceSearcher().addDeviceServer(&AndroidDeviceServer::instance());
    CLDeviceManager::instance().getDeviceSearcher().addDeviceServer(&IQEyeDeviceServer::instance());

	//=========================================================

	CLDevice::stopCommandProc();
}
