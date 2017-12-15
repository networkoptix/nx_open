#include "bookmark_search_list_model.h"
#include "private/bookmark_search_list_model_p.h"

namespace nx {
namespace client {
namespace desktop {

BookmarkSearchListModel::BookmarkSearchListModel(QObject* parent):
    base_type([this]() { return new Private(this); }, parent)
{
}

} // namespace
} // namespace client
} // namespace nx
