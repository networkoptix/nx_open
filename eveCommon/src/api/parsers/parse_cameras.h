#ifndef _eve_parse_cameras_
#define _eve_parse_cameras_

#include "device/qnresource.h"
#include "api/Types.h"

void parseCameras(QList<QnResourcePtr>& cameras, const QnApiCameras& xsdCameras);

#endif // _eve_parse_cameras_
