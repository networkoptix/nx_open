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

void QnApiSerializer::serialize(const QnResourcePtr& resource, QByteArray& data)
{
    try {
        if (resource.dynamicCast<QnVirtualCameraResource>()) {
            serializeCamera(resource.dynamicCast<QnVirtualCameraResource>(), data);
        } else if (resource.dynamicCast<QnVideoServerResource>()) {
            serializeServer(resource.dynamicCast<QnVideoServerResource>(), data);
        } else if (resource.dynamicCast<QnUserResource>()) {
            serializeUser(resource.dynamicCast<QnUserResource>(), data);
        }
    } catch (const std::exception& e) {
        qWarning() << "QnApiSerializer::serialize(): Exception caught. Message: "  << e.what();
        Q_ASSERT(0);
    }
}
