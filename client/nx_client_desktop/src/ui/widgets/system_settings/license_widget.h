#pragma once

#include <QtWidgets/QWidget>

#include <core/resource/resource_fwd.h>

#include <utils/common/connective.h>

namespace Ui {
class LicenseWidget;
}

class QnLicenseWidget : public Connective<QWidget>
{
    Q_OBJECT

    using base_type = Connective<QWidget>;
public:
    enum State
    {
        Normal,     /**< Operational state. */
        Waiting,    /**< Waiting for activation. */
    };

    explicit QnLicenseWidget(QWidget *parent = 0);
    virtual ~QnLicenseWidget();

    bool isOnline() const;
    void setOnline(bool online);

    bool isFreeLicenseAvailable() const;
    void setFreeLicenseAvailable(bool available);

    QString serialKey() const;
    void setSerialKey(const QString &serialKey);

    QByteArray activationKey() const;

    void setHardwareId(const QString &);

    State state() const;
    void setState(State state);

signals:
    void stateChanged();

protected:
    virtual void changeEvent(QEvent *event) override;

    private slots:
    void updateControls();

    void at_browseLicenseFileButton_clicked();

private:
    Q_DISABLE_COPY(QnLicenseWidget)

    QScopedPointer<Ui::LicenseWidget> ui;
    State m_state;
    QByteArray m_activationKey;
    bool m_freeLicenseAvailable;
    QnMediaServerResourcePtr m_currentServer;
};

