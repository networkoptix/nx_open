// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QScopedPointer>

#include <core/resource/resource_fwd.h>

#include <ui/dialogs/common/session_aware_dialog.h>

namespace Ui { class LicenseDetailsDialog; }

class QnLicenseDetailsDialog: public QnSessionAwareButtonBoxDialog
{
    Q_OBJECT
    using base_type = QnSessionAwareButtonBoxDialog;

public:
    QnLicenseDetailsDialog(const QnLicensePtr& license, QWidget* parent);
    virtual ~QnLicenseDetailsDialog() override;

private:
    QScopedPointer<Ui::LicenseDetailsDialog> ui;
};
