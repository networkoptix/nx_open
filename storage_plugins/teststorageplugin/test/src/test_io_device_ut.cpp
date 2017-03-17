#include <string.h>
#include <algorithm>
#include "test_common.h"


TEST_F(TestStorageTest, IODevice_open_Read)
{
    int ecode;
    IODeviceUniquePtr ioDevice1(
        dynamic_cast<TestIODeviceHelper*>(
            storage->open(
                "test://storage/some/path/hi_quality/someCameraId1/2016/01/23/15/1453550461076.mkv",
                nx_spl::io::ReadOnly,
                &ecode)), 
        ioDeviceDeleter);
    ASSERT_NE(ioDevice1.get(), nullptr);
}