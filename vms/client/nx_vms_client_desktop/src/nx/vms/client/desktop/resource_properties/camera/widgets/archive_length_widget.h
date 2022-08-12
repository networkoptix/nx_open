// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtWidgets/QWidget>

#include <nx/utils/impl_ptr.h>

namespace nx::vms::client::desktop {

class CameraSettingsDialogStore;
class Aligner;

class ArchiveLengthWidget: public QWidget
{
    Q_OBJECT
    using base_type = QWidget;

public:
    explicit ArchiveLengthWidget(QWidget* parent = nullptr);
    virtual ~ArchiveLengthWidget() override;

    /**
     * @return Aligner that line up 'Min' and 'Max' caption labels.
     */
    Aligner* aligner() const;

    void setStore(CameraSettingsDialogStore* store);

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::desktop
