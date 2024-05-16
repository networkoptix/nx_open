// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <functional>

#include <QtCore/QIdentityProxyModel>

#include <core/resource/resource_fwd.h>

namespace nx::vms::client::desktop {

/**
 * Proxy model which provides boolean type data by <tt>Qn::IsValidResourceRole</tt> role for each
 * leaf node of source model which provides valid resource pointer by <tt>core::ResourceRole</tt>
 * role. Used as source model for <tt>InvalidResourceFilterModel</tt>.
 */
class InvalidResourceDecoratorModel: public QIdentityProxyModel
{
    Q_OBJECT
    using base_type = QIdentityProxyModel;
    using ResourceValidator = std::function<bool(const QnResourcePtr&)>;

public:
    InvalidResourceDecoratorModel(
        const ResourceValidator& resourceValidator,
        QObject* parent = nullptr);

    virtual QVariant data(const QModelIndex& index, int role) const override;

    /**
     * @param resources Any set of pointers to resources.
     * @returns True if at least one resource from the set passed as parameter will be evaluated
     *     as invalid by encapsulated function wrapper.
     */
    bool hasInvalidResources(const QSet<QnResourcePtr>& resources) const;

private:
    ResourceValidator m_resourceValidator;
};

} // namespace nx::vms::client::desktop
