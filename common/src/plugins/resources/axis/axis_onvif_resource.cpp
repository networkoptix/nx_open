
#ifndef DISABLE_ONVIF

#include "axis_onvif_resource.h"

#include <QtCore/QMutexLocker>

#include "onvif/soapMediaBindingProxy.h"
#include <utils/network/http/asynchttpclient.h>

QnAxisOnvifResource::QnAxisOnvifResource()
{
}

int QnAxisOnvifResource::suggestBitrateKbps(Qn::StreamQuality q, QSize resolution, int fps) const
{
    Q_UNUSED(q)
    Q_UNUSED(resolution)
    Q_UNUSED(fps)
    return 0;
}

#endif  //ENABLE_ONVIF
