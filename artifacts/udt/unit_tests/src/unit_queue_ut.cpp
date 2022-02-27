#include <gtest/gtest.h>

#include <udt/unit_queue.h>

namespace test {

static constexpr int kUnitSize = 1440;
static constexpr int kUnitQueueSize = 10;

class UnitQueue:
	public ::testing::Test
{
public:
	UnitQueue():
		m_unitQueue(kUnitQueueSize, kUnitSize)
	{
	}

protected:
	std::shared_ptr<Unit> getNewUnit()
	{
		return m_unitQueue.takeNextAvailUnit();
	}

private:
	::UnitQueue m_unitQueue;
};

TEST_F(UnitQueue, unit_of_required_size_is_provided)
{
	const auto unit = getNewUnit();
	ASSERT_NE(nullptr, unit);
	ASSERT_EQ(kUnitSize, unit->packet().payload().size());
}

TEST_F(UnitQueue, unit_in_use_is_not_reused)
{
	const auto unit = getNewUnit();
	unit->setFlag(Unit::Flag::occupied);

	const auto unit2 = getNewUnit();
	ASSERT_NE(unit, unit2);
}

TEST_F(UnitQueue, unit_is_reused)
{
	auto unit = getNewUnit();
	unit->setFlag(Unit::Flag::occupied);
	const auto unitPtr = unit.get();
	unit = nullptr;

	std::vector<std::shared_ptr<Unit>> units;
	for (int i = 0; i < kUnitQueueSize * 10; ++i)
	{
		const auto unit2 = getNewUnit();
		if (unit2.get() == unitPtr)
			return;
		unit2->setFlag(Unit::Flag::occupied);
		units.push_back(unit2);
	}

	FAIL();
}

} // namespace test
