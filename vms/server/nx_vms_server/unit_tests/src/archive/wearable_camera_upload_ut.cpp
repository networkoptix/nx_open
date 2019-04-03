#include <gtest/gtest.h>

#include <test_support/mediaserver_with_storage_fixture.h>

namespace nx::vms::server::test {

class FtWearableCameraUpload: public test_support::MediaserverWithStorageFixture
{
protected:
    void givenSingleFileOnDisk()
    {
    }

    void whenWearableCameraIsCreated()
    {
    }

    void whenFileIsUploaded()
    {
    }

    void whenPlayArchiveRequestIsIssued()
    {
    }

    void thenArchiveShouldBePlayedWithoutGaps()
    {
    }

private:
};

TEST_F(FtWearableCameraUpload, UploadSingleFile)
{
    givenSingleFileOnDisk();
    whenServerStarted();
    thenArchiveShouldBeScannedCorreclty();

    whenWearableCameraIsCreated();
    whenFileIsUploaded();

    whenPlayArchiveRequestIsIssued();
    thenArchiveShouldBePlayedWithoutGaps();
}

} // namespace nx::vms::server::test