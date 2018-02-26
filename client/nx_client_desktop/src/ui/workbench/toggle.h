#ifndef QN_TOGGLE_H
#define QN_TOGGLE_H

#include <QtCore/QObject>

class QnToggle: public QObject {
    Q_OBJECT;
public:
    QnToggle(QObject *parent = NULL): QObject(parent), m_active(false) {}
    QnToggle(bool active, QObject *parent = NULL): QObject(parent), m_active(active) {}

    virtual ~QnToggle() {
        setActive(false);
    }

public slots:
    void setActive(bool active) {
        if(m_active == active)
            return;

        m_active = active;

        emit toggled(m_active);
        if(m_active) {
            emit activated();
        } else {
            emit deactivated();
        }
    }
    
    void setInactive(bool inactive) { setActive(!inactive); }
    void activate() { setActive(true); }
    void deactivate() { setActive(false); }

signals:
    void toggled(bool active);
    void activated();
    void deactivated();

private:
    bool m_active;
};


#endif // QN_TOGGLE_H
