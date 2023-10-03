// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QStringList>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QWidget>

#include <common/common_globals.h>
#include <core/resource/resource_fwd.h>
#include <nx/utils/uuid.h>
#include <nx/vms/client/core/common/utils/common_module_aware.h>

namespace Ui { class CameraExpertSettingsWidget; }

namespace nx::vms::client::desktop {

struct CameraSettingsDialogState;
class CameraSettingsDialogStore;
class Aligner;

class CameraExpertSettingsWidget:
    public QWidget,
    public nx::vms::client::core::CommonModuleAware
{
    Q_OBJECT
    using base_type = QWidget;

public:
    CameraExpertSettingsWidget(CameraSettingsDialogStore* store, QWidget* parent = nullptr);
    virtual ~CameraExpertSettingsWidget() override;

private:
    void loadState(const CameraSettingsDialogState& state);
    void clearSpinBoxSelection();

private:
    const QScopedPointer<Ui::CameraExpertSettingsWidget> ui;
    QList<Aligner*> m_aligners;
};

} // namespace nx::vms::client::desktop
