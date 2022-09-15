// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "layout_tour_state_manager.h"

#include <client_core/client_core_module.h>
#include <common/common_module.h>
#include <core/resource_management/layout_tour_manager.h>
#include <nx/vms/common/system_context.h>

QnLayoutTourStateManager::QnLayoutTourStateManager(QObject* parent):
    base_type(parent)
{
    connect(qnClientCoreModule->commonModule()->layoutTourManager(),
        &QnLayoutTourManager::tourRemoved,
        this,
        &QnAbstractSaveStateManager::clean);
}
