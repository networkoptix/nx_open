// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QEvent>
#include <QtGui/QKeyEvent>
#include <QtWidgets/QWidget>

#include <core/resource/resource_fwd.h>
#include <nx/utils/impl_ptr.h>
#include <ui/workbench/workbench_context_aware.h>

namespace Ui { class LicenseWidget; }

class QnLicenseWidget: public QWidget,
    public QnWorkbenchContextAware
{
    Q_OBJECT
    using base_type = QWidget;

public:
    enum State
    {
        Normal,     /**< Operational state. */
        Waiting,    /**< Waiting for activation. */
    };

    explicit QnLicenseWidget(QWidget* parent = nullptr);
    virtual ~QnLicenseWidget();

    bool isOnline() const;
    void setOnline(bool online);

    bool isFreeLicenseAvailable() const;
    void setFreeLicenseAvailable(bool available);
    bool isFreeLicenseKey() const;

    QString serialKey() const;
    void setSerialKey(const QString& serialKey);

    QByteArray activationKey() const;

    void setHardwareId(const QString&);

    State state() const;
    void setState(State state);

    void clearManualActivationUserInput();

signals:
    void licenseActivationRequested();

protected:
    virtual void changeEvent(QEvent* event) override;
    virtual void keyPressEvent(QKeyEvent* event) override;

private:
    void updateControls();
    QString calculateManualActivationLinkText() const;
    void updateManualActivationLinkText();
    void browseForLicenseFile();

private:
    nx::utils::ImplPtr<Ui::LicenseWidget> ui;
    State m_state = Normal;
    QByteArray m_activationKey;
    bool m_freeLicenseAvailable = true;
    QnMediaServerResourcePtr m_currentServer;
};
