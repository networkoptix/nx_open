#include <gtest/gtest.h>
#include <qcoreapplication.h>

#include "storage/abstract_storage_resource.h"

extern test::StorageTestGlobals tg;

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    tg.prepare();
    ::testing::InitGoogleTest(&argc, argv);
    const int result = RUN_ALL_TESTS();
    return result;
}
