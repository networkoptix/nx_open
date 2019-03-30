#include <gtest/gtest.h>

#include <test_support/mediaserver_with_storage_fixture.h>

namespace nx::vms::server::test {

class FtReindex: public test_support::MediaserverWithStorageFixture
{
};

TEST_F(FtReindex, FastArchiveScan_AllDataRetrieved)
{
    givenSomeArchiveOnHdd();
    whenServerStarted();
    thenArchiveShouldBeScannedCorreclty();
}

TEST_F(FtReindex, FastArchiveScan_PartialDataRetrieved)
{
    givenSomeArchiveOnHdd();
    whenServerStarted();
    thenArchiveShouldBeScannedCorreclty();

    whenServerStopped();
    whenSomeArchiveDataAdded();
    whenServerStarted();
    thenArchiveShouldBeScannedCorreclty();

    whenServerStopped();
    whenSomeArchiveDataAdded();
    whenServerStarted();
    thenArchiveShouldBeScannedCorreclty();
}

TEST_F(FtReindex, FullArchiveScan_AllDataRetrieved)
{
    whenServerStarted();
    thenArchiveShouldBeScannedCorreclty();

    whenSomeArchiveDataAdded();
    whenReindexRequestIssued();
    thenArchiveShouldBeScannedCorreclty();

    whenServerStopped();
    whenServerStarted();
    thenArchiveShouldBeScannedCorreclty();

    whenSomeArchiveDataAdded();
    whenReindexRequestIssued();
    thenArchiveShouldBeScannedCorreclty();
}

} // nx::vms::server::test
