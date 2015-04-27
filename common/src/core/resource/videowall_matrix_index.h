#ifndef VIDEOWALL_MATRIX_INDEX_H
#define VIDEOWALL_MATRIX_INDEX_H

#include <QtCore/QMetaType>
#include <utils/common/uuid.h>
#include <QtCore/QList>

#include <core/resource/resource_fwd.h>

/**
 * This class contains all the necessary information to look up a videowall matrix.
 */
class QnVideoWallMatrixIndex {
public:
    QnVideoWallMatrixIndex() {}

    QnVideoWallMatrixIndex(const QnVideoWallResourcePtr &videowall, const QnUuid &uuid):
        m_videowall(videowall), m_uuid(uuid)
    {}

    QnVideoWallResourcePtr videowall() const;
    void setVideoWall(const QnVideoWallResourcePtr &videowall);

    QnUuid uuid() const;
    void setUuid(const QnUuid &uuid);

    /** \return true if the index is not initialized. */
    bool isNull() const;

    /** \return true if the index contains valid videowall matrix data. */
    bool isValid() const;

    QnVideoWallMatrix matrix() const;


private:
    QnVideoWallResourcePtr m_videowall;
    QnUuid m_uuid;
};

Q_DECLARE_METATYPE(QnVideoWallMatrixIndex)
Q_DECLARE_METATYPE(QnVideoWallMatrixIndexList)


#endif // VIDEOWALL_MATRIX_INDEX_H
