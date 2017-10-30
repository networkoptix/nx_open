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

    void fetchMore();

private:
    class Private;
    QScopedPointer<Private> d;
};

} // namespace
} // namespace client
} // namespace nx
