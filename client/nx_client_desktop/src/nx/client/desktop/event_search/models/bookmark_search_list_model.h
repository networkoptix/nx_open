#pragma once

#include <nx/client/desktop/event_search/models/abstract_async_search_list_model.h>

namespace nx {
namespace client {
namespace desktop {

class BookmarkSearchListModel: public AbstractAsyncSearchListModel
{
    Q_OBJECT
    using base_type = AbstractAsyncSearchListModel;

public:
    explicit BookmarkSearchListModel(QObject* parent = nullptr);
    virtual ~BookmarkSearchListModel() override = default;

private:
    class Private;
};

} // namespace desktop
} // namespace client
} // namespace nx
