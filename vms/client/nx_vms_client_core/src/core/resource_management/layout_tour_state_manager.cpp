#include "layout_tour_state_manager.h"

#include <common/common_module.h>

#include <client_core/client_core_module.h>

#include <core/resource_management/layout_tour_manager.h>

QnLayoutTourStateManager::QnLayoutTourStateManager(QObject* parent):
    base_type(parent)
{
    connect(qnClientCoreModule->commonModule()->layoutTourManager(),
        &QnLayoutTourManager::tourRemoved,
        this,
        &QnAbstractSaveStateManager::clean);
}
