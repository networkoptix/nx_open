// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>

#include <QtCore/QVector>
#include <QtCore/QVariant>

namespace nx::vms::client::desktop {
namespace entity_item_model {

/**
 * Abstract interface for the single resource tree item.
 */
class NX_VMS_CLIENT_DESKTOP_API AbstractItem
{
public:
    AbstractItem();
    virtual ~AbstractItem();

    virtual QVariant data(int role) const = 0;
    virtual Qt::ItemFlags flags() const = 0;

    using DataChangedCallback = std::function<void(const QVector<int>& /*roles*/)>;
    void setDataChangedCallback(const DataChangedCallback& callback);

protected:
    void notifyDataChanged(const QVector<int>& roles) const;

private:
    DataChangedCallback m_dataChangedCallback;
};

using AbstractItemPtr = std::unique_ptr<AbstractItem>;

} // namespace entity_item_model
} // namespace nx::vms::client::desktop
