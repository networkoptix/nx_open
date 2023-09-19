// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <core/resource/resource_fwd.h>
#include <nx/utils/impl_ptr.h>
#include <nx/utils/uuid.h>

#include "types.h"

namespace nx::vms::client::desktop::figure {

/** Watches on figures and their updates for the specified camera. */
class RoiFiguresWatcher: public QObject
{
    Q_OBJECT
    using base_type = QObject;

public:
    static FiguresWatcherPtr create(
        const QnVirtualCameraResourcePtr& camera,
        QObject* parent = nullptr);

    RoiFiguresWatcher(
        const QnVirtualCameraResourcePtr& camera,
        QObject* parent = nullptr);
    virtual ~RoiFiguresWatcher() override;

    FigureInfosByEngine figures() const;

    FigurePtr figure(
        const EngineId& engineId,
        const FigureId& figureId) const;

    QString figureLabelText(
        const EngineId& engineId,
        const FigureId& figureId) const;

signals:
    void figureAdded(
        const EngineId& engineId,
        const FigureId& figureId);
    void figureChanged(
        const EngineId& engineId,
        const FigureId& figureId);
    void figureRemoved(
        const EngineId& engineId,
        const FigureId& figureId);

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::desktop::figure
