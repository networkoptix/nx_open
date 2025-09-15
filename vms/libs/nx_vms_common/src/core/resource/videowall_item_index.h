// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QList>

#include <core/resource/resource_fwd.h>
#include <nx/utils/uuid.h>

/**
 * This class contains all the necessary information to look up a videowall item.
 */
class NX_VMS_COMMON_API QnVideoWallItemIndex
{
public:
    QnVideoWallItemIndex();
    QnVideoWallItemIndex(const QnVideoWallResourcePtr& videowall, const nx::Uuid& uuid);

    QnVideoWallResourcePtr videowall() const;
    QnVideoWallItem item() const;
    nx::Uuid uuid() const;

    /** \return true if the index item is not initialized. */
    bool isNull() const;

    /** \return true if the index contains valid videowall item data. */
    bool isValid() const;

    /** Debug string representation. */
    QString toString() const;

    bool operator==(const QnVideoWallItemIndex&) const = default;

private:
    QnVideoWallResourcePtr m_videowall;
    nx::Uuid m_uuid;
};
