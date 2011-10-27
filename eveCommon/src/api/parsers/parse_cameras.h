#ifndef _eve_parse_cameras_
#define _eve_parse_cameras_

#include "api/Types.h"
#include "core/resource/resource.h"

void parseCameras(QList<QnResourcePtr>& cameras, const QnApiCameras& xsdCameras);

#endif // _eve_parse_cameras_
