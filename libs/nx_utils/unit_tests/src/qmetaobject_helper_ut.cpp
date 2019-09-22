#include <gtest/gtest.h>

#include "qmetaobject_helper_ut.h"
#include <nx/utils/qmetaobject_helper.h>

namespace nx::utils::test {

TestObjectWithSignal::TestObjectWithSignal(QObject* parent):
    QThread(parent),
    m_testInt(1),
    m_testId(QUuid().createUuid()),
    m_testRect(QRect(1, 2, 3, 4))
{
    moveToThread(this);
    connect(this, &QThread::started,
        [this]()
        {
            emitAsync(this, "signal1", m_testInt);
            emitAsync(this, "signal2", m_testId);
            emitAsync(this, "signal3", m_testRect, m_testId, 1);
        });

    connect(this, &TestObjectWithSignal::signal1, this, &TestObjectWithSignal::slot1);
    connect(this, &TestObjectWithSignal::signal2, this, &TestObjectWithSignal::slot2);
    connect(this, &TestObjectWithSignal::signal3, this, &TestObjectWithSignal::slot3);
}

void TestObjectWithSignal::slot1(int id)
{
    if (id == m_testInt)
        ++m_processedSlots;
    emitDoneIfNeed();
}

void TestObjectWithSignal::slot2(QUuid id)
{
    if (id == m_testId)
        ++m_processedSlots;
    emitDoneIfNeed();
}

void TestObjectWithSignal::slot3(QRect rect, QUuid id, int value)
{
    if (rect == m_testRect && id == m_testId && value == m_testInt)
        ++m_processedSlots;
    emitDoneIfNeed();
}

void TestObjectWithSignal::emitDoneIfNeed()
{
    if (m_processedSlots == 3)
        emit done();
}

TEST(EmitAsyncTest, basic)
{
    auto object = std::make_unique<TestObjectWithSignal>();
    std::promise<void> promise;

    QObject::connect(object.get(), &TestObjectWithSignal::done,
        [&promise, object = object.get()]() { promise.set_value(); });

    object->start();
    promise.get_future().wait();
    object->exit();
    object->wait();
}

} // namespace nx::utils::test
