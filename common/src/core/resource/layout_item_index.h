#ifndef QN_LAYOUT_ITEM_INDEX_H
#define QN_LAYOUT_ITEM_INDEX_H

#include <QtCore/QMetaType>
#include <utils/common/uuid.h>
#include <QtCore/QList>
#include "resource_fwd.h"

/**
 * This class contains all the necessary information to look up a layout item. 
 */
class QnLayoutItemIndex {
public:
    QnLayoutItemIndex() {}

    QnLayoutItemIndex(const QnLayoutResourcePtr &layout, const QnUuid &uuid):
        m_layout(layout), m_uuid(uuid) 
    {}

    const QnLayoutResourcePtr &layout() const {
        return m_layout;
    }

    void setLayout(const QnLayoutResourcePtr &layout) {
        m_layout = layout;
    }

    const QnUuid &uuid() const {
        return m_uuid;
    }

    void setUuid(const QnUuid &uuid) {
        m_uuid = uuid;
    }

    bool isNull() const {
        return m_layout.isNull() || m_uuid.isNull();
    }

private:
    QnLayoutResourcePtr m_layout;
    QnUuid m_uuid;
};

Q_DECLARE_METATYPE(QnLayoutItemIndex);

Q_DECLARE_METATYPE(QnLayoutItemIndexList);

#endif // QN_LAYOUT_ITEM_INDEX_H
