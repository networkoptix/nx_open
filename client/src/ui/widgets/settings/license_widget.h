#ifndef LICENSEWIDGET_H
#define LICENSEWIDGET_H

#include <QtGui/QWidget>

namespace Ui {
    class LicenseWidget;
}

class LicenseWidget : public QWidget
{
    Q_OBJECT

public:
    enum State {
        Normal,     /**< Operational state. */
        Waiting,    /**< Waiting for activation. */
    };

    explicit LicenseWidget(QWidget *parent = 0);
    virtual ~LicenseWidget();

    bool isOnline() const;
    void setOnline(bool online);

    bool isFreeLicenseAvailable() const;
    void setFreeLicenseAvailable(bool available);

    QString serialKey() const;
    void setSerialKey(const QString &serialKey);

    QByteArray activationKey() const;

    void setHardwareId(const QByteArray &);

    State state() const;
    void setState(State state);

signals:
    void stateChanged();

protected:
    virtual void changeEvent(QEvent *event) override;

private slots:
    void updateControls();

    void at_browseLicenseFileButton_clicked();
    void at_activateLicenseButton_clicked();
    void at_activateFreeLicenseButton_clicked();
    void at_activationTypeComboBox_currentIndexChanged();

private:
    Q_DISABLE_COPY(LicenseWidget)

    QScopedPointer<Ui::LicenseWidget> ui;
    State m_state;
};

#endif // LICENSEWIDGET_H
