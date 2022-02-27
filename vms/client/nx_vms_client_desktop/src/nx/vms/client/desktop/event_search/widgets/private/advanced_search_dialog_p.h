// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "../advanced_search_dialog.h"

#include <optional>

#include <QtCore/QSize>

#include <nx/utils/singleton.h>
#include <nx/vms/client/desktop/state/client_state_handler.h>

namespace nx::vms::client::desktop {

class AdvancedSearchDialog::StateDelegate:
    public QObject,
    public ClientStateDelegate,
    public Singleton<StateDelegate>
{
    Q_OBJECT

public:
    QSize windowSize;
    std::optional<QPoint> windowPosition;
    bool isMaximized = false;
    int screen = -1;

signals:
    void loaded();
    void aboutToBeSaved();

protected:
    virtual bool loadState(
        const DelegateState& state, SubstateFlags flags, const StartupParameters& params) override;

    virtual void saveState(DelegateState* state, SubstateFlags flags) override;
};

} // namespace nx::vms::client::desktop
