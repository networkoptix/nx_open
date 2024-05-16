// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/utils/impl_ptr.h>

#include "abstract_async_search_list_model.h"

class QnCommonModule;

namespace nx::vms::client::core {

class NX_VMS_CLIENT_CORE_API BookmarkSearchListModel: public AbstractAsyncSearchListModel
{
    Q_OBJECT
    using base_type = AbstractAsyncSearchListModel;

public:
    explicit BookmarkSearchListModel(
        int maximumCount,
        SystemContext* systemContext,
        QObject* parent = nullptr);

    explicit BookmarkSearchListModel(
        SystemContext* systemContext,
        QObject* parent = nullptr);

    virtual ~BookmarkSearchListModel() override;

    virtual TextFilterSetup* textFilter() const override;

    virtual bool isConstrained() const override;
    virtual bool hasAccessRights() const override;

    Q_INVOKABLE void dynamicUpdate(const qint64& centralPointMs);

    virtual int rowCount(const QModelIndex &parent = QModelIndex()) const override;

    virtual QVariant data(const QModelIndex& index, int role) const override;

protected:
    virtual bool requestFetch(
        const FetchRequest& request,
        const FetchCompletionHandler& completionHandler) override;

    virtual void clearData() override;

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::core
