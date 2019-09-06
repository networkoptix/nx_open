#pragma once

#include <QObject>

namespace nx::vms::rules {

class NX_VMS_RULES_API Field: public QObject
{
    Q_OBJECT

public:
    Field();

    Q_INVOKABLE virtual QString metatype() const = 0;

    void connectSignals();

    QMap<QString, QJsonValue> serializedProperties() const;

    bool event(QEvent* ev) override;

signals:
	void stateChanged();

private:
    void notifyParent();

private:
    bool m_connected = false;
    bool m_updateInProgress = false;

};

} // namespace nx::vms::rules
