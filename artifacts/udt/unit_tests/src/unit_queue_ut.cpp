#include <gtest/gtest.h>

#include <udt/unit_queue.h>

namespace test {

static constexpr int kUnitQueueSize = 10;

class UnitQueue:
	public ::testing::Test
{
public:
	UnitQueue():
		m_unitQueue(kUnitQueueSize)
	{
	}

protected:
	Unit getNewUnit()
	{
		return m_unitQueue.takeNextAvailUnit();
	}

private:
	::UnitQueue m_unitQueue;
};

TEST_F(UnitQueue, unit_in_use_is_not_reused)
{
	auto unit = getNewUnit();
	unit.setFlag(Unit::Flag::occupied);

	auto unit2 = getNewUnit();
	ASSERT_NE(&unit.packet(), &unit2.packet());
}

TEST_F(UnitQueue, unit_is_reused)
{
	std::optional<Unit> unit(getNewUnit());
	unit->setFlag(Unit::Flag::occupied);
	const auto packet1Ptr = unit->packet().get();
	unit.reset();

	std::vector<Unit> units;
	for (int i = 0; i < kUnitQueueSize * 10; ++i)
	{
		auto unit2 = getNewUnit();
		if (unit2.packet().get() == packet1Ptr)
			return;
		unit2.setFlag(Unit::Flag::occupied);
		units.push_back(std::move(unit2));
	}

	FAIL();
}

} // namespace test
