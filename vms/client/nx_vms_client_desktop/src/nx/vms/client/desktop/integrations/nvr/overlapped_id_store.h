// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <nx/vms/client/desktop/common/flux/flux_types.h>

namespace nx::vms::client::desktop::integrations {

struct OverlappedIdState;

class NX_VMS_CLIENT_DESKTOP_API OverlappedIdStore: public QObject
{
    Q_OBJECT
    using base_type = QObject;

public:
    explicit OverlappedIdStore(QObject* parent = nullptr);
    virtual ~OverlappedIdStore() override;

    void reset();
    void setCurrentId(int id);
    void setIdList(const std::vector<int>& idList);

    const OverlappedIdState& state() const;

signals:
    void stateChanged(const OverlappedIdState& state);

private:
    struct Private;
    QScopedPointer<Private> d;
};

} // namespace nx::vms::client::desktop::integrations
