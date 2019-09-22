#pragma once

#include <QtCore/QThread>
#include <QtCore/QUuid>
#include <QtCore/QRect>

namespace nx::utils::test {

class TestObjectWithSignal: public QThread
{
    Q_OBJECT
public:
    TestObjectWithSignal(QObject* parent = nullptr);
signals:
    void signal1(int id);
    void signal2(QUuid id);
    void signal3(QRect rect, QUuid id, int value);

    void done();
public slots:
    void slot1(int id);
    void slot2(QUuid id);
    void slot3(QRect rect, QUuid id, int value);
private:
    void emitDoneIfNeed();
private:
    mutable QnMutex m_mutex;
    std::promise<void> m_promise;
    int m_processedSlots = 0;

    const int m_testInt = 0;
    const QUuid m_testId;
    const QRect m_testRect;
};

} // namespace nx::utils::test
