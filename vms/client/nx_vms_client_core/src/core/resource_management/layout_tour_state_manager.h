// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <core/resource_management/abstract_save_state_manager.h>

class QnLayoutTourStateManager: public QnAbstractSaveStateManager
{
    Q_OBJECT

    using base_type = QnAbstractSaveStateManager;
public:
    QnLayoutTourStateManager(QObject* parent = nullptr);

};
