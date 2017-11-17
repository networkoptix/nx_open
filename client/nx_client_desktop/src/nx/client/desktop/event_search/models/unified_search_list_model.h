#pragma once

#include <QtCore/QScopedPointer>

#include <core/resource/resource_fwd.h>

#include <nx/client/desktop/event_search/models/event_list_model.h>

namespace nx {
namespace client {
namespace desktop {

class UnifiedSearchListModel: public EventListModel
{
    Q_OBJECT
    using base_type = EventListModel;

public:
    explicit UnifiedSearchListModel(QObject* parent = nullptr);
    virtual ~UnifiedSearchListModel() override;

    QnVirtualCameraResourcePtr camera() const;
    void setCamera(const QnVirtualCameraResourcePtr& camera);

    enum class Filter
    {
        all,
        events,
        bookmarks,
        analytics
    };

    Filter filter() const;
    void setFilter(Filter filter);

    virtual bool canFetchMore(const QModelIndex& parent = QModelIndex()) const override;
    virtual void fetchMore(const QModelIndex& parent = QModelIndex()) override;

private:
    class Private;
    QScopedPointer<Private> d;
};

} // namespace
} // namespace client
} // namespace nx
