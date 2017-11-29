#pragma once

#include <QtCore/QScopedPointer>

#include <core/resource/resource_fwd.h>

#include <nx/client/desktop/common/models/concatenation_list_model.h>
#include <nx/vms/event/event_fwd.h>

namespace nx {
namespace client {
namespace desktop {

class UnifiedSearchListModel: public ConcatenationListModel
{
    Q_OBJECT
    using base_type = ConcatenationListModel;

public:
    explicit UnifiedSearchListModel(QObject* parent = nullptr);
    virtual ~UnifiedSearchListModel() override;

    QnVirtualCameraResourcePtr camera() const;
    void setCamera(const QnVirtualCameraResourcePtr& camera);

    enum class Type
    {
        events = 0x1,
        bookmarks = 0x2,
        analytics = 0x4,
        all = events | bookmarks | analytics
    };
    Q_DECLARE_FLAGS(Types, Type);

    Types filter() const;
    void setFilter(Types filter);

    vms::event::EventType selectedEventType() const;
    void setSelectedEventType(vms::event::EventType value);

    virtual bool canFetchMore(const QModelIndex& parent = QModelIndex()) const override;
    virtual void fetchMore(const QModelIndex& parent = QModelIndex()) override;

private:
    using base_type::setModels;

private:
    class Private;
    QScopedPointer<Private> d;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(UnifiedSearchListModel::Types);

} // namespace
} // namespace client
} // namespace nx
