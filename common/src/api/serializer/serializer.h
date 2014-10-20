#ifndef QN_API_SERIALIZER_H
#define QN_API_SERIALIZER_H

#include <QtCore/QByteArray>
#include <QtCore/QList>
#include <QtGui/QRegion>

class QnMotionRegion;

//TODO: #GDM #Common rename/move this module to something sane or get rid of it

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
