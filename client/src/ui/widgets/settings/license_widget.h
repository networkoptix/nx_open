#ifndef QN_LICENSE_WIDGET_H
#define QN_LICENSE_WIDGET_H

#include <QtWidgets/QWidget>

namespace Ui {
    class LicenseWidget;
}

class QnLicenseWidget : public QWidget {
    Q_OBJECT

public:
    enum State {
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
    Q_DISABLE_COPY(QnLicenseWidget)

    QScopedPointer<Ui::LicenseWidget> ui;
    State m_state;
    QByteArray m_activationKey;
};

#endif // QN_LICENSE_WIDGET_H
