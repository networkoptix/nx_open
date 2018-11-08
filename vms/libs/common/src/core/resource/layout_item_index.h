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
    QnLayoutItemIndex() {}

    QnLayoutItemIndex(const QnLayoutResourcePtr& layout, const QnUuid& uuid):
        m_layout(layout), m_uuid(uuid)
    {
    }

    const QnLayoutResourcePtr& layout() const
    {
        return m_layout;
    }

    void setLayout(const QnLayoutResourcePtr& layout)
    {
        m_layout = layout;
    }

    const QnUuid& uuid() const
    {
        return m_uuid;
    }

    void setUuid(const QnUuid& uuid)
    {
        m_uuid = uuid;
    }

    bool isNull() const
    {
        return m_layout.isNull() || m_uuid.isNull();
    }

private:
    QnLayoutResourcePtr m_layout;
    QnUuid m_uuid;
};

Q_DECLARE_METATYPE(QnLayoutItemIndex);
Q_DECLARE_METATYPE(QnLayoutItemIndexList);
