#include "onvif_resource_settings.h"

QnOnvifResourceSettings::QnOnvifResourceSettings()
{

}

QnOnvifResourceSettings::~QnOnvifResourceSettings()
{

}

bool QnOnvifResourceSettings::setBrightness(float val)
{
    return true;
}

float QnOnvifResourceSettings::getBrightness()
{
    return -1;
}

bool QnOnvifResourceSettings::setSharpness(float val)
{
    return true;
}

float QnOnvifResourceSettings::getSharpness()
{
    return -1;
}

bool QnOnvifResourceSettings::setColorSaturation(float val)
{
    return true;
}

float QnOnvifResourceSettings::getColorSaturation()
{
    return -1;
}

bool QnOnvifResourceSettings::setWhiteBalance(WhiteBalance balance)
{
    return true;
}

WhiteBalance QnOnvifResourceSettings::setWhiteBalance()
{
    return WhiteBalance();
}

bool QnOnvifResourceSettings::setExposure(Exposure exp)
{
    return true;
}

Exposure QnOnvifResourceSettings::setExposure()
{
    return Exposure();
}

bool QnOnvifResourceSettings::reboot()
{
    return true;
}

bool QnOnvifResourceSettings::softReset()
{
    return true;
}

bool QnOnvifResourceSettings::hardReset()
{
    return true;
}
