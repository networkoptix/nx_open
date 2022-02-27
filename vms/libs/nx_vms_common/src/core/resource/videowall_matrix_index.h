// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QList>
#include <QtCore/QMetaType>

#include <core/resource/resource_fwd.h>

#include <nx/utils/uuid.h>

/**
 * This class contains all the necessary information to look up a videowall matrix.
 */
class NX_VMS_COMMON_API QnVideoWallMatrixIndex
{
public:
    QnVideoWallMatrixIndex();
    QnVideoWallMatrixIndex(const QnVideoWallResourcePtr& videowall, const QnUuid& uuid);

    QnVideoWallResourcePtr videowall() const;
    void setVideoWall(const QnVideoWallResourcePtr& videowall);

    QnUuid uuid() const;
    void setUuid(const QnUuid& uuid);

    /** \return true if the index is not initialized. */
    bool isNull() const;

    /** \return true if the index contains valid videowall matrix data. */
    bool isValid() const;

    QnVideoWallMatrix matrix() const;

    /** Debug string representation. */
    QString toString() const;

private:
    QnVideoWallResourcePtr m_videowall;
    QnUuid m_uuid;
};

Q_DECLARE_METATYPE(QnVideoWallMatrixIndex)
Q_DECLARE_METATYPE(QnVideoWallMatrixIndexList)
