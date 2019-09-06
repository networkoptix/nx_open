#pragma once

#include <QObject>

namespace nx::vms::rules {

class NX_VMS_RULES_API Field: public QObject
{
    Q_OBJECT

public:
    Field();
    void connectSignals();
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
