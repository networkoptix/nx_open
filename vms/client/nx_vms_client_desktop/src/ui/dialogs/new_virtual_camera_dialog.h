// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <ui/dialogs/common/session_aware_dialog.h>
#include <core/resource/resource_fwd.h>

namespace Ui { class NewVirtualCameraDialog; }

class QnResourceListModel;

class QnNewVirtualCameraDialog: public QnSessionAwareButtonBoxDialog
{
    Q_OBJECT
    using base_type = QnSessionAwareButtonBoxDialog;

public:
    explicit QnNewVirtualCameraDialog(QWidget* parent, const QnMediaServerResourcePtr& selectedServer = {});
    virtual ~QnNewVirtualCameraDialog() override;

    QString name() const;
    const QnMediaServerResourcePtr server() const;

    virtual void accept() override;

private:
    QScopedPointer<Ui::NewVirtualCameraDialog> ui;
    QnResourceListModel* m_model = nullptr;
};
