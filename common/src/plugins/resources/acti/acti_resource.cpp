
#include "acti_resource.h"

#include <functional>
#include <memory>

#include "api/app_server_connection.h"
#include "../onvif/dataprovider/onvif_mjpeg.h"
#include "acti_stream_reader.h"
#include <business/business_event_connector.h>
#include <business/business_event_rule.h>
#include <business/business_rule_processor.h>
#include "utils/common/synctime.h"


using namespace std;

const char* QnActiResource::MANUFACTURE = "ACTI";
static const float MAX_AR_EPS = 0.04f;
static const quint64 MOTION_INFO_UPDATE_INTERVAL = 1000000ll * 60;
static const quint16 DEFAULT_AXIS_API_PORT = 80;

QnActiResource::QnActiResource()
{
    setAuth(QLatin1String("root"), QLatin1String("123456"));
}

QnActiResource::~QnActiResource()
{
}

QString QnActiResource::manufacture() const
{
    return QLatin1String(MANUFACTURE);
}

void QnActiResource::setIframeDistance(int /*frames*/, int /*timems*/)
{

}

QnAbstractStreamDataProvider* QnActiResource::createLiveDataProvider()
{
    return new QnActiStreamReader(toSharedPointer());
}

bool QnActiResource::shoudResolveConflicts() const 
{
    return false;
}

void QnActiResource::setCropingPhysical(QRect /*croping*/)
{

}

bool QnActiResource::initInternal()
{
    return false;
}

bool QnActiResource::isResourceAccessible()
{
    return updateMACAddress();
}

const QnResourceAudioLayout* QnActiResource::getAudioLayout(const QnAbstractStreamDataProvider* dataProvider)
{
    if (isAudioEnabled()) {
        const QnActiStreamReader* actiReader = dynamic_cast<const QnActiStreamReader*>(dataProvider);
        if (actiReader && actiReader->getDPAudioLayout())
            return actiReader->getDPAudioLayout();
        else
            return QnPhysicalCameraResource::getAudioLayout(dataProvider);
    }
    else
        return QnPhysicalCameraResource::getAudioLayout(dataProvider);
}
