// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <chrono>
#include <expected>
#include <filesystem>
#include <fstream>
#include <functional>
#include <future>
#include <memory>
#include <optional>
#include <ostream>
#include <ranges>
#include <string>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

#include <nx/ranges/std_compat.h>
#include <nx/utils/test_support/test_options.h>

#include "archive_reader.h"
#include "dir_contents_manager.h"
#include "stream_reader.h"

namespace {

using namespace std::chrono;

// Bounds a call that may block forever; a call returning normally takes microseconds.
constexpr milliseconds kBlockedCallLimit = 5s;

// The plugin serves the files as they are, so the content does not have to be a real image.
std::expected<std::filesystem::path, std::string> makeImageFolder(
    const std::filesystem::path& dir, int imageCount)
{
    std::error_code error;
    std::filesystem::create_directories(dir, error);
    if (error)
        return std::unexpected("Unable to create " + dir.string() + ": " + error.message());

    for (const int i: std::views::iota(0, imageCount))
    {
        const auto image = dir / ("image_" + std::to_string(i) + ".jpg");
        if (!(std::ofstream(image, std::ios::binary) << "jpeg"))
            return std::unexpected("Unable to write " + image.string());
    }
    return dir;
}

struct FramesRead
{
    std::vector<int> codes; //< The result of each getNextData() call.
    microseconds elapsed; //< The time all the calls took together.
};

// NOTE: Blocks until all the frames are read; a live reader serves them one frame duration apart.
FramesRead readFrames(nxcip::StreamReader* reader, int frameCount)
{
    return nx::ranges::fold_left(std::views::iota(0, frameCount),
        FramesRead{},
        [reader](FramesRead read, int)
        {
            const auto start = steady_clock::now();
            nxcip::MediaDataPacket* packet = nullptr;
            read.codes.push_back(reader->getNextData(&packet));
            if (packet)
                packet->releaseRef();
            read.elapsed += duration_cast<microseconds>(steady_clock::now() - start);
            return read;
        });
}

constexpr int kImageCount = 5;

struct ReverseModeInput
{
    std::optional<microseconds> position; //< Where to jump; nullopt keeps the current position.
    bool forward = true; //< The requested direction; the reader starts forward at the first frame.
};

// Positions are offsets from the first frame; the folder holds one frame per second:
//     0s, 1s, 2s, 3s, 4s
// Forward selects the frame at or after the position, reverse the frame at or before it:
//     {2.5s} -> 3s
//     {2.5s, reverse} -> 2s
//     {nullopt, reverse} -> 0s //< Keeps the current position: the first frame.
struct ReverseModeCase
{
    std::string name;
    ReverseModeInput input;
    microseconds expect; //< The position the reader selects.
};

void PrintTo(const ReverseModeCase& c, std::ostream* os)
{
    *os << c.name;
}

class ArchiveReverseMode: public ::testing::TestWithParam<ReverseModeCase>
{
protected:
    static constexpr microseconds kFrameDuration = 1s;

    static void SetUpTestSuite()
    {
        imageFolder = makeImageFolder(
            std::filesystem::path(
                nx::TestOptions::temporaryDirectoryPath(/*canCreate*/ true).toStdString())
                / "archive",
            kImageCount);
    }

    static void TearDownTestSuite() { imageFolder = {}; }

    // Per case: a blocked case leaves its reader locked for good.
    void SetUp() override
    {
        if (!imageFolder)
            FAIL() << imageFolder.error();

        dirContents =
            std::make_shared<DirContentsManager>(imageFolder->string(), kFrameDuration.count());
        reader = std::shared_ptr<ArchiveReader>(
            new ArchiveReader(dirContents.get(), kFrameDuration.count()),
            std::mem_fn(&ArchiveReader::releaseRef));
        start = reader->startTime();
        // The rows assume the timeline from the ReverseModeCase comment: kImageCount frames, one
        // per kFrameDuration.
        ASSERT_EQ(((kImageCount - 1) * kFrameDuration).count(), reader->endTime() - start);
    }

    static inline std::expected<std::filesystem::path, std::string> imageFolder;

    std::shared_ptr<DirContentsManager> dirContents;
    std::shared_ptr<ArchiveReader> reader; //< Refers to dirContents.
    nxcip::UsecUTCTimestamp start = nxcip::INVALID_TIMESTAMP_VALUE; //< The first frame.
};

TEST_P(ArchiveReverseMode, SelectsPosition)
{
    const auto& [name, input, expect] = GetParam();
    const std::shared_future selected = std::async(std::launch::async,
        [dirContents = this->dirContents, reader = this->reader, start = this->start, input]
        {
            nxcip::UsecUTCTimestamp position = nxcip::INVALID_TIMESTAMP_VALUE;
            reader->setReverseMode(/*cSeq*/ 1,
                !input.forward,
                input.position ? start + input.position->count() : nxcip::INVALID_TIMESTAMP_VALUE,
                &position);
            return microseconds(position - start);
        }).share();
    // The last reference to an async state blocks on destruction until the call returns; this one
    // outlives the test, so a blocked call fails the case instead of hanging the test.
    std::thread([selected] { selected.wait(); }).detach();

    // A blocked call never becomes ready, so get() must not be reached.
    ASSERT_EQ(std::future_status::ready, selected.wait_for(kBlockedCallLimit))
        << "Blocked for " << kBlockedCallLimit.count() << "ms";
    EXPECT_EQ(expect, selected.get());
}

INSTANTIATE_TEST_SUITE_P(ImageLibraryPlugin,
    ArchiveReverseMode,
    ::testing::ValuesIn(std::vector<ReverseModeCase>{
        {
            .name = "KeepForwardWithoutPosition",
            .input = {.position = std::nullopt},
            .expect = 0s,
        },
        {
            .name = "ReverseWithoutPosition",
            .input = {.position = std::nullopt, .forward = false},
            .expect = 0s,
        },
        {
            .name = "ForwardAtFrame",
            .input = {.position = 2s},
            .expect = 2s,
        },
        {
            .name = "ReverseAtFrame",
            .input = {.position = 2s, .forward = false},
            .expect = 2s,
        },
        {
            .name = "ForwardBetweenFrames",
            .input = {.position = 2500ms},
            .expect = 3s,
        },
        {
            .name = "ReverseBetweenFrames",
            .input = {.position = 2500ms, .forward = false},
            .expect = 2s,
        },
    }),
    /*nameGenerator*/ [](const auto& info) { return info.param.name; });

class LiveStream: public ::testing::Test
{
protected:
    static void SetUpTestSuite()
    {
        imageFolder = makeImageFolder(
            std::filesystem::path(
                nx::TestOptions::temporaryDirectoryPath(/*canCreate*/ true).toStdString())
                / "live",
            kImageCount);
    }

    static void TearDownTestSuite() { imageFolder = {}; }

    void SetUp() override
    {
        if (!imageFolder)
            FAIL() << imageFolder.error();
    }

    static inline std::expected<std::filesystem::path, std::string> imageFolder;
};

// Live mode emulates a camera: a frame is served no earlier than one frame duration after the
// previous one, so the first frame comes at once and each next one waits.
TEST_F(LiveStream, FramesArePaced)
{
    constexpr microseconds frameDuration = 100ms;
    constexpr int frameCount = 4;
    const std::vector kExpectedCodes(frameCount, nxcip::NX_NO_ERROR);
    DirContentsManager dirContents(imageFolder->string(), frameDuration.count());
    nxpt::CommonRefManager owner(static_cast<nxpl::PluginInterface*>(nullptr));
    StreamReader reader(
        &owner, &dirContents, frameDuration.count(), /*liveMode*/ true, /*encoderNumber*/ 0);

    const auto [codes, elapsed] = readFrames(&reader, frameCount);

    EXPECT_EQ(kExpectedCodes, codes);
    EXPECT_LE((frameCount - 1) * frameDuration, elapsed);
}

} // namespace
