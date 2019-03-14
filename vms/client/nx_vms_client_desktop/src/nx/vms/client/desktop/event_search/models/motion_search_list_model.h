#pragma once

#include <QtCore/QList>
#include <QtGui/QRegion>

#include <nx/vms/client/desktop/event_search/models/abstract_async_search_list_model.h>

namespace nx::vms::client::desktop {

class MotionSearchListModel: public AbstractAsyncSearchListModel
{
    Q_OBJECT
    using base_type = AbstractAsyncSearchListModel;

public:
    explicit MotionSearchListModel(QnWorkbenchContext* context, QObject* parent = nullptr);
    virtual ~MotionSearchListModel() override = default;

    QList<QRegion> filterRegions() const; //< One region per channel.
    void setFilterRegions(const QList<QRegion>& value);

    bool isFilterEmpty() const;

    virtual bool isConstrained() const override;

protected:
    virtual bool isCameraApplicable(const QnVirtualCameraResourcePtr& camera) const override;

private:
    class Private;
    Private* const d;
};

} // namespace nx::vms::client::desktop
