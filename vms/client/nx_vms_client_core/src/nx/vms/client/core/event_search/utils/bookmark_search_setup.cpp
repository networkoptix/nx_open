// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "bookmark_search_setup.h"

#include <QtQml/QtQml>

#include <nx/vms/client/core/event_search/models/bookmark_search_list_model.h>

namespace nx::vms::client::core {

void BookmarkSearchSetup::registerQmlType()
{
    qmlRegisterUncreatableType<BookmarkSearchSetup>("nx.vms.client.core", 1, 0,
        "BookmarkSearchSetup", "Cannot create instance of core BookmarkSearchSetup");
}

BookmarkSearchSetup::BookmarkSearchSetup(
    BookmarkSearchListModel* model,
    QObject* parent)
    :
    base_type(parent),
    m_model(model)
{
    if (!NX_ASSERT(m_model))
        return;

    connect(m_model, &BookmarkSearchListModel::searchSharedOnlyChanged,
        this, &BookmarkSearchSetup::parametersChanged);
}

BookmarkSearchSetup::~BookmarkSearchSetup()
{
}

bool BookmarkSearchSetup::searchSharedOnly() const
{
    return NX_ASSERT(m_model) && m_model->searchSharedOnly();
}

void BookmarkSearchSetup::setSearchSharedOnly(const bool value)
{
    if (NX_ASSERT(m_model))
        m_model->setSearchSharedOnly(value);
}

} // namespace nx::vms::client::core
