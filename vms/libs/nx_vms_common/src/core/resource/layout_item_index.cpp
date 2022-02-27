// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "layout_item_index.h"

#include <core/resource/layout_resource.h>

#include <nx/utils/log/log.h>

QnLayoutItemIndex::QnLayoutItemIndex()
{
}

QnLayoutItemIndex::QnLayoutItemIndex(const QnLayoutResourcePtr& layout, const QnUuid& uuid):
    m_layout(layout),
    m_uuid(uuid)
{
}

const QnLayoutResourcePtr& QnLayoutItemIndex::layout() const
{
    return m_layout;
}

void QnLayoutItemIndex::setLayout(const QnLayoutResourcePtr& layout)
{
    m_layout = layout;
}

const QnUuid& QnLayoutItemIndex::uuid() const
{
    return m_uuid;
}

void QnLayoutItemIndex::setUuid(const QnUuid& uuid)
{
    m_uuid = uuid;
}

bool QnLayoutItemIndex::isNull() const
{
    return m_layout.isNull() || m_uuid.isNull();
}

QString QnLayoutItemIndex::toString() const
{
    return nx::format("QnLayoutItemIndex %1 - %2").args(m_layout, m_uuid);
}
