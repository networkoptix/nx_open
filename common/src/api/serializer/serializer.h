#ifndef QN_API_SERIALIZER_H
#define QN_API_SERIALIZER_H

#include <QtCore/QByteArray>

#include <api/model/kvpair.h>
#include <api/model/email_attachment.h>
#include <api/model/connection_info.h>

#include <business/business_fwd.h>

#include <core/resource/resource_fwd.h>
#include <core/resource/camera_history.h>
#include <core/resource/layout_item_data.h>
#include <core/resource/motion_window.h>

#include <licensing/license.h>

#include <utils/common/exception.h>

// TODO: #Elric move somewhere.

/*
 * Helper serialization functions. Not related to any specific serialization format.
 */

void parseRegion(QRegion& region, const QString& regionString);
void parseRegionList(QList<QRegion>& regions, const QString& regionsString);

void parseMotionRegion(QnMotionRegion& region, const QByteArray& regionString);
QString serializeMotionRegion(const QnMotionRegion& region);

void parseMotionRegionList(QList<QnMotionRegion>& regions, const QByteArray& regionsString);
QString serializeMotionRegionList(const QList<QnMotionRegion>& regions);

QString serializeRegion(const QRegion& region);
QString serializeRegionList(const QList<QRegion>& regions);


#endif // QN_API_SERIALIZER_H
