#ifndef VIDEO_WALL_PC_DATA_H
#define VIDEO_WALL_PC_DATA_H

#include <QtCore/QHash>
#include <QtCore/QList>
#include <QtCore/QMetaType>
#include <utils/common/uuid.h>
#include <QtCore/QRect>

class QnVideoWallPcData
{
public:
    class PcScreen {
    public:
        PcScreen() {}

        /** Index of the screen in the Virtual Desktop. */
        int index;

        /** Position and size of the screen in the Virtual Desktop coordinate system. */
        QRect desktopGeometry;

        /**
         * Position and size of the screen in the videowall review layout.
         * Can be null if this screen shares an item and is managed by another screen.
         */
        QRect layoutGeometry;

        friend bool operator==(const PcScreen &l, const PcScreen &r) {
            return (l.index == r.index &&
                    l.desktopGeometry == r.desktopGeometry &&
                    l.layoutGeometry == r.layoutGeometry);
        }
    };

    QnVideoWallPcData() {}

    QnUuid uuid;
    QList<PcScreen> screens;

    friend bool operator==(const QnVideoWallPcData &l, const QnVideoWallPcData &r) {
        return (l.uuid == r.uuid &&
                l.screens == r.screens);
    }
};

Q_DECLARE_METATYPE(QnVideoWallPcData::PcScreen)
Q_DECLARE_TYPEINFO(QnVideoWallPcData::PcScreen, Q_MOVABLE_TYPE);

Q_DECLARE_METATYPE(QnVideoWallPcData)
Q_DECLARE_TYPEINFO(QnVideoWallPcData, Q_MOVABLE_TYPE);

typedef QList<QnVideoWallPcData> QnVideoWallPcDataList;
typedef QHash<QnUuid, QnVideoWallPcData> QnVideoWallPcDataMap;

Q_DECLARE_METATYPE(QnVideoWallPcDataList)
Q_DECLARE_METATYPE(QnVideoWallPcDataMap)

#endif // VIDEO_WALL_PC_DATA_H
