// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QIdentityProxyModel>

class QnResourceIconCache;

namespace nx::vms::client::desktop {

/**
* Decorator model which provides icon by Qt::DecorationRole if source model provides valid data
* by Qn::ResourceIconKeyRole. Any testable model shouldn't provide icons itself, since
* QnResourceIconCache class isn't and shouldn't be instantiated within testing environment.
*/
class ResourceTreeIconDecoratorModel: public QIdentityProxyModel
{
    Q_OBJECT
    using base_type = QIdentityProxyModel;

public:
    ResourceTreeIconDecoratorModel();
    virtual ~ResourceTreeIconDecoratorModel() override;

    /**
     * @returns True if resource status icon decoration is displayed. Default value: true.
     */
    bool displayResourceStatus() const;

    /**
     * Sets whether resource status icon decoration should be drawn or not.
     */
    void setDisplayResourceStatus(bool display);

    virtual QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;

private:
    QnResourceIconCache* m_resourceIconCache;
    bool m_displayResourceStatus = true;
};

} // namespace nx::vms::client::desktop
