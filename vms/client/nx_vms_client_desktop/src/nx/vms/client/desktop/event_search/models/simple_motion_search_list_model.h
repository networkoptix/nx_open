// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once


#include <nx/utils/impl_ptr.h>
#include <nx/vms/client/core/common/data/motion_selection.h>
#include <nx/vms/client/core/event_search/event_search_globals.h>
#include <nx/vms/client/core/event_search/models/abstract_search_list_model.h>
#include <nx/vms/client/desktop/window_context_aware.h>

class QMenu;

namespace nx::vms::client::desktop {

/**
 * Motion search list model for current workbench navigator resource.
 */
class SimpleMotionSearchListModel: public core::AbstractSearchListModel,
    public WindowContextAware
{
    Q_OBJECT
    Q_PROPERTY(bool filterEmpty READ isFilterEmpty NOTIFY filterRegionsChanged)

    using base_type = core::AbstractSearchListModel;
    using MotionSelection = nx::vms::client::core::MotionSelection;

public:
    explicit SimpleMotionSearchListModel(WindowContext* context, QObject* parent = nullptr);
    virtual ~SimpleMotionSearchListModel() override;

    MotionSelection filterRegions() const;
    void setFilterRegions(const MotionSelection& value);
    bool isFilterEmpty() const;
    Q_INVOKABLE void clearFilterRegions();

    virtual bool isConstrained() const override;
    virtual bool hasAccessRights() const override;

    virtual int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    virtual QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;

signals:
    void filterRegionsChanged();

protected:
    virtual bool requestFetch(
        const core::FetchRequest& request,
        const FetchCompletionHandler& completionHandler) override;

    virtual bool cancelFetch() override;

    virtual bool isFilterDegenerate() const override;

    virtual void clearData() override;

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::desktop
