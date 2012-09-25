#include "serializer.h"

void parseRegion(QRegion& region, const QString& regionString)
{
    foreach (QString rectString, regionString.split(QLatin1Char(';')))
    {
        QRect rect;
        QStringList rectList = rectString.split(QLatin1Char(','));

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
    QStringList regList = regionsString.split(QLatin1Char(':'));
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
    QStringList regList = regionsString.split(QLatin1Char(':'));
    regions.clear();
    // for compatibility with previous version. By default screen filled medium sensitivity motion window
    for (int i = 0; i < regList.size(); ++i)
    {
        QnMotionRegion region;
        //region.addRect((QnMotionRegion::MAX_SENSITIVITY - QnMotionRegion::MIN_SENSITIVITY)/2, QRect(0,0,MD_WIDTH, MD_HEIGHT));
        regions << region;
    }

    for (int i = 0; i < regList.size(); ++i)
        parseMotionRegion(regions[i], regList[i]);
}

void parseMotionRegion(QnMotionRegion& region, const QString& regionString)
{
    QList<QRect> motionMask;
    bool firstRect = true;
    foreach (QString rectString, regionString.split(QLatin1Char(';')))
    {
        QStringList rectList = rectString.split(QLatin1Char(','));
        QRect r;
        int sensitivity = 0;
        if (rectList.size() == 4)
        {
            // for compatibility with version 1.0. Version 1.1 uses 5 parameters (sensitivity is added)
            r.setLeft(rectList[0].toInt());
            r.setTop(rectList[1].toInt());
            r.setWidth(rectList[2].toInt());
            r.setHeight(rectList[3].toInt());
        }
        else if (rectList.size() == 5)
        {
            if (firstRect) {
                region.removeDefaultMotion();
                firstRect = false;
            }
            sensitivity = rectList[0].toInt();
            r.setLeft(rectList[1].toInt());
            r.setTop(rectList[2].toInt());
            r.setWidth(rectList[3].toInt());
            r.setHeight(rectList[4].toInt());
        }
        if (sensitivity > 0)
            region.addRect(sensitivity, r);
        else
            motionMask << r;
    }
    for (int i = 0; i < motionMask.size(); ++i)
        region.addRect(0, motionMask[i]);
}

QString serializeMotionRegion(const QnMotionRegion& region)
{
    QStringList regionList;

    //foreach (const QnMotionWindow& window, region)
    for (int i = QnMotionRegion::MIN_SENSITIVITY; i <= QnMotionRegion::MAX_SENSITIVITY; ++i)
    {
        if (!region.getRegionBySens(i).isEmpty())
        {
            foreach(const QRect& rect, region.getRectsBySens(i))
            {
                QStringList rectList;
                rectList << QString::number(i) << QString::number(rect.left()) << QString::number(rect.top()) << QString::number(rect.width()) << QString::number(rect.height());
                regionList << rectList.join(QLatin1String(","));
            }
        }            
    }

    return regionList.join(QLatin1String(";"));
}

QString serializeRegion(const QRegion& region)
{
    QStringList regionList;

    foreach (const QRect& rect, region.rects())
    {
        QStringList rectList;
        rectList << QString::number(rect.left()) << QString::number(rect.top()) << QString::number(rect.width()) << QString::number(rect.height());
        regionList << rectList.join(QLatin1String(","));
    }

    return regionList.join(QLatin1String(";"));
}

QString serializeRegionList(const QList<QRegion>& regions)
{
    QString result;
    for (int i = 0; i < regions.size(); ++i)
    {
        QString regStr = serializeRegion(regions[i]);
        if (i > 0)
            result += QLatin1Char(':');
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
            result += QLatin1Char(':');
        result += regStr;
    }
    return result;
}

void QnApiSerializer::serialize(const QnResourcePtr& resource, QByteArray& data)
{
    try {
        if (resource.dynamicCast<QnVirtualCameraResource>()) {
            serializeCamera(resource.dynamicCast<QnVirtualCameraResource>(), data);
        } else if (resource.dynamicCast<QnMediaServerResource>()) {
            serializeServer(resource.dynamicCast<QnMediaServerResource>(), data);
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
