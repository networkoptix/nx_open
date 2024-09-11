// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <core/resource/resource_fwd.h>
#include <nx/utils/impl_ptr.h>
#include <ui/widgets/common/abstract_preferences_widget.h>
#include <ui/workbench/workbench_context_aware.h>

class QLabel;

namespace Ui {
    class ServerSettingsWidget;
}

class QnServerSettingsWidget:
    public QnAbstractPreferencesWidget, public QnWorkbenchContextAware
{
    Q_OBJECT
    using base_type = QnAbstractPreferencesWidget;

public:
    QnServerSettingsWidget(QWidget* parent = nullptr);
    virtual ~QnServerSettingsWidget();

    virtual bool hasChanges() const override;
    virtual void loadDataToUi() override;
    virtual void applyChanges() override;
    virtual void discardChanges() override;
    virtual bool canApplyChanges() const override;
    virtual bool isNetworkRequestRunning() const override;

    QnMediaServerResourcePtr server() const;
    void setServer(const QnMediaServerResourcePtr &server);

protected:
    void setReadOnlyInternal(bool readOnly) override;

private:
    void updateUrl();
    void updateReadOnly();
    void updateRemoteAccess();
    void updateWebCamerasDiscoveryEnabled();
    void updateCertificatesInfo();
    void updateDigestAlertBar();
    void showCertificateMismatchBanner(bool dataLoaded);
    int updateCertificatesLabels();

    bool isWebCamerasDiscoveryEnabled() const;
    bool isWebCamerasDiscoveryEnabledChanged() const;
    bool isCertificateMismatch() const;

    bool currentIsWebCamerasDiscoveryEnabled() const;

private slots:
    void at_pingButton_clicked();
    void showServerCertificate(const QString& id);

private:
    QScopedPointer<Ui::ServerSettingsWidget> ui;

    class Private;
    nx::utils::ImplPtr<Private> d;

    QnMediaServerResourcePtr m_server;
    QString m_initServerName;
};
