#pragma once

#include <QtCore/QObject>

/**
 * Class QnPendingOperation executes the given callback
 * when requestOperation() is called but
 * not more often than the given interval.
 */
class QnPendingOperation : public QObject {
public:
    typedef std::function<void ()> Callback;

    QnPendingOperation(const Callback &callback, int interval, QObject *parent);

    void requestOperation();

private:
    Callback m_callback;
    QTimer *m_timer;
    bool m_requested;
};
