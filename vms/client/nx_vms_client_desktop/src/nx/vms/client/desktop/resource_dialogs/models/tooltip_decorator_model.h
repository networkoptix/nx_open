// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <functional>

#include <QtCore/QIdentityProxyModel>

namespace nx::vms::client::desktop {

class TooltipDecoratorModel: public QIdentityProxyModel
{
    Q_OBJECT
    using base_type = QIdentityProxyModel;

public:
    TooltipDecoratorModel(QObject* parent = nullptr);
    virtual QVariant data(const QModelIndex& index, int role) const override;

    using TooltipProvider = std::function<QString(const QModelIndex&)>;
    void setTooltipProvider(const TooltipProvider& tooltipProvider);

private:
    TooltipProvider m_tooltipProvider;
};

} // namespace nx::vms::client::desktop
