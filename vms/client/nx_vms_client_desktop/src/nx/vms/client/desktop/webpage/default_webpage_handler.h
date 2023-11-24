// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <core/resource/resource_fwd.h>
#include <ui/workbench/workbench_context_aware.h>

namespace nx::vms::client::desktop {

class DefaultWebpageHandler: public QObject, public QnWorkbenchContextAware
{
public:
    DefaultWebpageHandler(QnWorkbenchContext* context, QObject* parent = nullptr);

private:
    void openDefaultWebPage(const QnWebPageResourcePtr& webpage);
};

} // namespace nx::vms::client::desktop
