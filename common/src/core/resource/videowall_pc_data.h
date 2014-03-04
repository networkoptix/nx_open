#ifndef VIDEO_WALL_PC_DATA_H
#define VIDEO_WALL_PC_DATA_H

#include <QtCore/QHash>
#include <QtCore/QList>
#include <QtCore/QMetaType>
#include <QtCore/QUuid>
#include <QtCore/QRect>

class QnVideoWallPcData
{
public:
    class PcScreen {
    public:
        PcScreen() {}

        int index;
        QRect geometry;

        friend bool operator==(const PcScreen &l, const PcScreen &r) {
            return (l.index == r.index &&
                    l.geometry == r.geometry);
        }
    };

    QnVideoWallPcData() {}

    QUuid uuid;
    QRect geometry;
    QList<PcScreen> screens;

    QRect unitedGeometry() const {
        QRect result;
        foreach (PcScreen screen, screens) {
            result = result.united(screen.geometry);
        }
        return result;
    }

    friend bool operator==(const QnVideoWallPcData &l, const QnVideoWallPcData &r) {
        return (l.uuid == r.uuid &&
                l.geometry == r.geometry &&
                l.screens == r.screens);
    }
};

Q_DECLARE_METATYPE(QnVideoWallPcData::PcScreen)
Q_DECLARE_TYPEINFO(QnVideoWallPcData::PcScreen, Q_MOVABLE_TYPE);

Q_DECLARE_METATYPE(QnVideoWallPcData)
Q_DECLARE_TYPEINFO(QnVideoWallPcData, Q_MOVABLE_TYPE);

typedef QList<QnVideoWallPcData> QnVideoWallPcDataList;
typedef QHash<QUuid, QnVideoWallPcData> QnVideoWallPcDataMap;

Q_DECLARE_METATYPE(QnVideoWallPcDataList)
Q_DECLARE_METATYPE(QnVideoWallPcDataMap)

#endif // VIDEO_WALL_PC_DATA_H
