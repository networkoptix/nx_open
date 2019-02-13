#pragma once

#include <QtCore/QEvent>
#include <QtWidgets/QWidget>

#include <core/resource/resource_fwd.h>
#include <utils/common/connective.h>

#include <nx/utils/impl_ptr.h>

namespace Ui { class LicenseWidget; }

class QnLicenseWidget: public Connective<QWidget>
{
    Q_OBJECT
    using base_type = Connective<QWidget>;

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

    QString serialKey() const;
    void setSerialKey(const QString& serialKey);

    QByteArray activationKey() const;

    void setHardwareId(const QString&);

    State state() const;
    void setState(State state);

signals:
    void stateChanged();

protected:
    virtual void changeEvent(QEvent* event) override;

private:
    void updateControls();
    void browseForLicenseFile();

private:
    nx::utils::ImplPtr<Ui::LicenseWidget> ui;
    State m_state = Normal;
    QByteArray m_activationKey;
    bool m_freeLicenseAvailable = true;
    QnMediaServerResourcePtr m_currentServer;
};
