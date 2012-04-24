#include "serializer.h"

void parseRegion(QRegion& region, const QString& regionString)
{
    foreach (QString rectString, regionString.split(';'))
    {
        QRect rect;
        QStringList rectList = rectString.split(',');

        if (rectList.size() == 4)
        {
            rect.setLeft(rectList[0].toInt());
            rect.setTop(rectList[1].toInt());
            rect.setWidth(rectList[2].toInt());
            rect.setHeight(rectList[3].toInt());
        }
        region += rect;
    }
}

void parseRegionList(QList<QRegion>& regions, const QString& regionsString)
{
    QStringList regList = regionsString.split(':');
    regions.clear();
    foreach(const QString& regionStr, regList)
    {
        QRegion region;
        parseRegion(region, regionStr);
        regions << region;
    }
}


void parseMotionRegionList(QList<QnMotionRegion>& regions, const QString& regionsString)
{
    QStringList regList = regionsString.split(':');
    regions.clear();
    foreach(const QString& regionStr, regList)
    {
        QnMotionRegion region;
        parseMotionRegion(region, regionStr);
        regions << region;
    }
}

void parseMotionRegion(QnMotionRegion& region, const QString& regionString)
{
    foreach (QString rectString, regionString.split(';'))
    {
        QStringList rectList = rectString.split(',');
        QRect r;
        int sensitivity = 0;
        if (rectList.size() == 4)
        {
            r.setLeft(rectList[0].toInt());
            r.setTop(rectList[1].toInt());
            r.setWidth(rectList[2].toInt());
            r.setHeight(rectList[3].toInt());
        }
        else if (rectList.size() == 5)
        {
            sensitivity = rectList[0].toInt();
            r.setLeft(rectList[1].toInt());
            r.setTop(rectList[2].toInt());
            r.setWidth(rectList[3].toInt());
            r.setHeight(rectList[4].toInt());
        }
        region.addRect(sensitivity, r);
    }
}

QString serializeMotionRegion(const QnMotionRegion& region)
{
    QStringList regionList;

    //foreach (const QnMotionWindow& window, region)
    for (int i = QnMotionRegion::MIN_SENSITIVITY; i <= QnMotionRegion::MAX_SENSITIVITY; ++i)
    {
        if (!region.getRegionBySens(i).isEmpty())
        {
            QStringList rectList;
            foreach(const QRect& rect, region.getRegionBySens(i).rects())
            {
                rectList << QString::number(rect.left()) << QString::number(rect.top()) << QString::number(rect.width()) << QString::number(rect.height());
                regionList << rectList.join(",");
            }
            if (i != QnMotionRegion::MIN_SENSITIVITY)
                regionList.last() = QString::number(i) + QString(",") + regionList.last();
        }            
    }

    return regionList.join(";");
}

QString serializeRegion(const QRegion& region)
{
    QStringList regionList;

    foreach (const QRect& rect, region.rects())
    {
        QStringList rectList;
        rectList << QString::number(rect.left()) << QString::number(rect.top()) << QString::number(rect.width()) << QString::number(rect.height());
        regionList << rectList.join(",");
    }

    return regionList.join(";");
}

QString serializeRegionList(const QList<QRegion>& regions)
{
    QString result;
    for (int i = 0; i < regions.size(); ++i)
    {
        QString regStr = serializeRegion(regions[i]);
        if (i > 0)
            result += ':';
        result += regStr;
    }
    return result;
}

QString serializeMotionRegionList(const QList<QnMotionRegion>& regions)
{
    QString result;
    for (int i = 0; i < regions.size(); ++i)
    {
        QString regStr = serializeMotionRegion(regions[i]);
        if (i > 0)
            result += ':';
        result += regStr;
    }
    return result;
}

void QnApiSerializer::serialize(const QnResourcePtr& resource, QByteArray& data)
{
    try {
        if (resource.dynamicCast<QnVirtualCameraResource>()) {
            serializeCamera(resource.dynamicCast<QnVirtualCameraResource>(), data);
        } else if (resource.dynamicCast<QnVideoServerResource>()) {
            serializeServer(resource.dynamicCast<QnVideoServerResource>(), data);
        } else if (resource.dynamicCast<QnUserResource>()) {
            serializeUser(resource.dynamicCast<QnUserResource>(), data);
        } else if (resource.dynamicCast<QnLayoutResource>()) {
            serializeLayout(resource.dynamicCast<QnLayoutResource>(), data);
        }
    } catch (const std::exception& e) {
        qCritical() << "QnApiSerializer::serialize(): Exception caught. Message: "  << e.what();
        Q_ASSERT(0);
    }
}
