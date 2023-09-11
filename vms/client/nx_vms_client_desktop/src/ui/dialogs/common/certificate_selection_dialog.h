// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/utils/impl_ptr.h>
#include <ui/dialogs/common/session_aware_dialog.h>

namespace Ui {
class CertificateSelectionDialog;
} // namespace Ui

class QQmlListReference;

namespace nx::vms::client::desktop {

/**
 * Client certificate selectioh dialog that will be automatically closed on client disconnect.
 */
class CertificateSelectionDialog: public QnSessionAwareButtonBoxDialog
{
    Q_OBJECT
    using base_type = QnSessionAwareButtonBoxDialog;

public:
    CertificateSelectionDialog(
        QWidget* parent,
        const QUrl& hostUrl,
        const QQmlListReference& certificates);

    virtual ~CertificateSelectionDialog() override;

    int selectedIndex() const;

private:
    nx::utils::ImplPtr<Ui::CertificateSelectionDialog> ui;
};

} // namespace nx::vms::client::desktop
