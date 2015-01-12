/**********************************************************
* 8 sep 2014
* a.kolesnikov
***********************************************************/

#include <gtest/gtest.h>

#include <QCoreApplication>

#include <utils/common/log.h>


int main( int argc, char **argv )
{
#if 0
    QCoreApplication app( argc, argv );

    QnLog::initLog("DEBUG2");
    cl_log.create(
        "C:\\tmp\\ut\\log_file",
        10*1024*1024,
        25,
        QnLogLevel::cl_logDEBUG2 );
#endif

    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
