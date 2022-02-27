// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/utils/impl_ptr.h>
#include <ui/dialogs/common/dialog.h>

namespace nx::vms::client::core { struct CloudAuthData; }

namespace nx::vms::client::desktop {

class OauthLoginDialogPrivate;

class OauthLoginDialog: public QnDialog
{
    Q_OBJECT
    using base_type = QnDialog;

public:
    static nx::vms::client::core::CloudAuthData login(
        QWidget* parent,
        const QString& title,
        const QString& clientType,
        bool sessionAware,
        const QString& cloudSystem = QString());

    static bool validateToken(
        QWidget* parent,
        const QString& title,
        const std::string& token);

    OauthLoginDialog(
        QWidget* parent,
        const QString& clientType,
        const QString& cloudSystem = QString());

    virtual ~OauthLoginDialog() override;

    const nx::vms::client::core::CloudAuthData& authData() const;

signals:
    void authDataReady();

private:
    void loadPage();

private:
    nx::utils::ImplPtr<OauthLoginDialogPrivate> d;
};

} // namespace nx::vms::client::desktop
