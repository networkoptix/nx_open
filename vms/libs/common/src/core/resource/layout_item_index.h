#pragma once

#include <QtCore/QList>
#include <QtCore/QMetaType>

#include <core/resource/resource_fwd.h>

#include <nx/utils/uuid.h>

/**
 * This class contains all the necessary information to look up a layout item.
 */
class QnLayoutItemIndex
{
public:
    QnLayoutItemIndex();
    QnLayoutItemIndex(const QnLayoutResourcePtr& layout, const QnUuid& uuid);

    const QnLayoutResourcePtr& layout() const;
    void setLayout(const QnLayoutResourcePtr& layout);

    const QnUuid& uuid() const;
    void setUuid(const QnUuid& uuid);

    bool isNull() const;

    /** Debug string representation. */
    QString toString() const;

private:
    QnLayoutResourcePtr m_layout;
    QnUuid m_uuid;
};

Q_DECLARE_METATYPE(QnLayoutItemIndex);
Q_DECLARE_METATYPE(QnLayoutItemIndexList);
