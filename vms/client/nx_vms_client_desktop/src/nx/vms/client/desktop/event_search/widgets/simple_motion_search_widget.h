// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QList>
#include <QtGui/QRegion>

#include <nx/utils/impl_ptr.h>
#include <nx/vms/client/core/common/data/motion_selection.h>

#include "abstract_search_widget.h"

namespace nx::vms::client::desktop {

class SimpleMotionSearchListModel;

class SimpleMotionSearchWidget: public AbstractSearchWidget
{
    Q_OBJECT
    using base_type = AbstractSearchWidget;
    using MotionSelection = nx::vms::client::core::MotionSelection;

public:
    SimpleMotionSearchWidget(WindowContext* context, QWidget* parent = nullptr);
    virtual ~SimpleMotionSearchWidget() override;

    SimpleMotionSearchListModel* motionModel() const;

private:
    virtual QString placeholderText(bool constrained) const override;
    virtual QString itemCounterText(int count) const override;
    virtual bool calculateAllowance() const override;

private:
    class Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::desktop
