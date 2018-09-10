#pragma once

#include <nx/client/desktop/event_search/models/abstract_async_search_list_model.h>

namespace nx::client::desktop {

class BookmarkSearchListModel: public AbstractAsyncSearchListModel
{
    Q_OBJECT
    using base_type = AbstractAsyncSearchListModel;

public:
    explicit BookmarkSearchListModel(QObject* parent = nullptr);
    virtual ~BookmarkSearchListModel() override = default;

    QString filterText() const;
    void setFilterText(const QString& value);

    virtual bool isConstrained() const override;

private:
    class Private;
    Private* const d = nullptr;
};

} // namespace nx::client::desktop
