
#include "axis_onvif_resource.h"

#include <QMutexLocker>

#include "onvif/soapMediaBindingProxy.h"
#include <utils/network/http/asynchttpclient.h>

QnAxisOnvifResource::QnAxisOnvifResource()
{
}

int QnAxisOnvifResource::suggestBitrateKbps(QnStreamQuality q, QSize resolution, int fps) const
{
    Q_UNUSED(q)
    Q_UNUSED(resolution)
    Q_UNUSED(fps)
    return 0;
}
