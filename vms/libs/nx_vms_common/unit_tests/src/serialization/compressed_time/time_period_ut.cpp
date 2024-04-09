// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

/**********************************************************
* 26 feb 2015
* r.vasilenko
***********************************************************/

#include <QDateTime>
#include <iomanip>
#include <iostream>
#include <random>

#include <gtest/gtest.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

#include <QtCore/QUuid>

#include <nx/fusion/fusion/fusion_adaptor.h>
#include <nx/fusion/serialization/compressed_time_functions.h>
#include <nx/utils/gzip/gzip_compressor.h>
#include <recording/time_period.h>
#include <recording/time_period_list.h>

namespace {

class TimePeriodCompressedSerializationTest
    :
    public ::testing::Test
{
protected:
    static void SetUpTestCase()
    {
    }

    static void TearDownTestCase()
    {
    }
};


TEST( TimePeriodCompressedSerializationTest, SerializeCompressedTimePeriodsUnsigned)
{

    std::vector<QnTimePeriod> srcData;
    quint64 currentTime = QDateTime::currentMSecsSinceEpoch();
    srcData.push_back(QnTimePeriod(currentTime, 30 * 1000ll));
    currentTime += 40 * 1000ll;
    srcData.push_back(QnTimePeriod(currentTime, 120 * 1000ll));
    currentTime += 1000 * 1000ll;
    srcData.push_back(QnTimePeriod(currentTime, 1000000ll));
    currentTime += 1000000ll;
    srcData.push_back(QnTimePeriod(3000000000ll * 1000ll, 5));

    QByteArray buffer;
    std::vector<QnTimePeriod> dstData;

    // 1. unsigned stream test
    {
        QnCompressedTimeWriter<QByteArray> inputStream(&buffer, false);
        QnCompressedTime::serialize( srcData, &inputStream);
    }

    {
        QnCompressedTimeReader<QByteArray> outputStream(&buffer);
        QnCompressedTime::deserialize( &outputStream, &dstData);
    }

    ASSERT_TRUE( dstData == srcData );
}


// 2. signed stream test
TEST( TimePeriodCompressedSerializationTest, SerializeCompressedTimePeriodsSigned)
{
    std::vector<QnTimePeriod> srcData;
    std::vector<QnTimePeriod> dstData;
    QByteArray buffer;

    quint64 currentTime = QDateTime::currentMSecsSinceEpoch();
    srcData.push_back(QnTimePeriod(currentTime, 30 * 1000ll));
    currentTime += 20 * 1000ll;
    srcData.push_back(QnTimePeriod(currentTime, 20 * 1000ll));
    currentTime += 1000 * 1000ll;
    srcData.push_back(QnTimePeriod(currentTime, 1000000ll));
    srcData.push_back(QnTimePeriod(currentTime, 1000000ll));
    srcData.push_back(QnTimePeriod(3000000000ll * 1000ll, 5));

    {
        QnCompressedTimeWriter<QByteArray> inputStream(&buffer, true);
        QnCompressedTime::serialize( srcData, &inputStream);
    }

    {
        QnCompressedTimeReader<QByteArray> outputStream(&buffer);
        QnCompressedTime::deserialize( &outputStream, &dstData);
    }

    ASSERT_TRUE( dstData == srcData );

}

TEST( TimePeriodCompressedSerializationTest, SerializeCompressedMultiServerPeriodData)
{
    MultiServerPeriodDataList srcData;

    MultiServerPeriodData data;

    quint64 currentTime = QDateTime::currentMSecsSinceEpoch();
    data.periods.push_back(QnTimePeriod(currentTime, 30 * 1000ll));
    currentTime += 40 * 1000ll;
    data.periods.push_back(QnTimePeriod(currentTime, 120 * 1000ll));
    currentTime += 1000 * 1000ll;
    data.periods.push_back(QnTimePeriod(currentTime, 1000000ll));
    currentTime += 1000000ll;
    data.periods.push_back(QnTimePeriod(3000000000ll * 1000ll, 5));

    srcData.push_back(std::move(data));

    QByteArray buffer = QnCompressedTime::serialized(srcData, false);

    bool success = false;
    MultiServerPeriodDataList dstData = QnCompressedTime::deserialized(buffer, MultiServerPeriodDataList(), &success);

    ASSERT_TRUE( success );
    ASSERT_TRUE( dstData == srcData );
}

TEST( TimePeriodCompressedSerializationTest, SerializeCompressedMultiServerPeriodDataListUnsigned)
{
    MultiServerPeriodDataList srcData;

    for (int i = 0; i < 5; ++i) {
        MultiServerPeriodData data;

        quint64 currentTime = QDateTime::currentMSecsSinceEpoch();
        data.periods.push_back(QnTimePeriod(currentTime, 30 * 1000ll));
        currentTime += 40 * 1000ll;
        data.periods.push_back(QnTimePeriod(currentTime, 120 * 1000ll));
        currentTime += 1000 * 1000ll;
        data.periods.push_back(QnTimePeriod(currentTime, 1000000ll));
        currentTime += 1000000ll;
        data.periods.push_back(QnTimePeriod(3000000000ll * 1000ll, 5));

        srcData.push_back(std::move(data));
    }


    QByteArray buffer = QnCompressedTime::serialized(srcData, false);

    bool success = false;
    MultiServerPeriodDataList dstData = QnCompressedTime::deserialized(buffer, MultiServerPeriodDataList(), &success);

    ASSERT_TRUE( success );
    ASSERT_TRUE( dstData == srcData );
}

TEST( TimePeriodCompressedSerializationTest, SerializeCompressedMultiServerPeriodDataListSignedMinimal)
{
    MultiServerPeriodDataList srcData;
    MultiServerPeriodData data;

    data.periods.push_back(QnTimePeriod(1429891331259, 1));
    data.periods.push_back(QnTimePeriod(1433773026421, 1));
    srcData.push_back(std::move(data));

    QByteArray buffer = QnCompressedTime::serialized(srcData, true);

    bool success = false;
    MultiServerPeriodDataList dstData = QnCompressedTime::deserialized(buffer, MultiServerPeriodDataList(), &success);

    ASSERT_TRUE( success );
    ASSERT_TRUE( dstData == srcData );
}

TEST( TimePeriodCompressedSerializationTest, SerializeCompressedMultiServerPeriodDataListSigned)
{
    MultiServerPeriodDataList srcData;

    MultiServerPeriodData data;
    data.guid = nx::Uuid::createUuid();

    data.periods.push_back(QnTimePeriod(1429889084026, 1353727));
    data.periods.push_back(QnTimePeriod(1429891331259, 864863));
    data.periods.push_back(QnTimePeriod(1433773026421, 12088254));
    data.periods.push_back(QnTimePeriod(1433841105581, 13334769));
    data.periods.push_back(QnTimePeriod(1434386547349, 5359406));
    data.periods.push_back(QnTimePeriod(1434456216257, 587665));
    srcData.push_back(std::move(data));

    QByteArray buffer = QnCompressedTime::serialized(srcData, true);

    bool success = false;
    MultiServerPeriodDataList dstData = QnCompressedTime::deserialized(buffer, MultiServerPeriodDataList(), &success);

    ASSERT_TRUE( success );
    ASSERT_TRUE( dstData == srcData );
}

class TimePeriodCompressionBenchmark: public ::testing::Test
{
protected:
    virtual void SetUp() override
    {
        // todo: make the test parameterized
        const size_t n = 1'000'000;
        _periods = generatePeriods({.n = n});
    }

    [[nodiscard]] const std::vector<QnTimePeriod>& periods() const { return _periods; }

    struct Report
    {
        size_t chunks;
        size_t sizeBytes;
        std::chrono::milliseconds min;
        std::chrono::milliseconds max;
        std::chrono::milliseconds median;
    };

    // F must accept `const std::vector<QnTimePeriods>&`
    template<typename F>
    [[nodiscard]] Report run(size_t sampleSize, F f) const
    {
        using std::chrono::milliseconds;
        using std::chrono::steady_clock;

        std::vector<milliseconds> results{};
        results.reserve(sampleSize);
        size_t sizeBytes = 0;

        for (size_t i = 0; i < sampleSize; ++i)
        {
            const auto start = steady_clock::now();

            // run iteration
            // defer destruction of the compressed buffer to not include the time taken
            const auto compressed = f(periods());

            const auto elapsed =
                std::chrono::duration_cast<milliseconds>(steady_clock::now() - start);
            sizeBytes = compressed.size();
            results.emplace_back(elapsed);
        }

        std::sort(results.begin(), results.end());
        return {.sizeBytes = sizeBytes,
            .min = results.front(),
            .max = results.back(),
            .median = results[std::midpoint<size_t>(0, results.size())]};
    }

    template <size_t N>
    void printReports(const std::array<std::pair<std::string_view, Report>, N>& reports) const
    {
        const auto diffPerc =
            [](const auto& ref, const auto& val) -> double
            {
              if (!val)
                  return 100.0;
              const auto r = static_cast<double>(ref);
              const auto v = static_cast<double>(val);

              return (v / r) * 100.0;
            };

        std::cout << NX_FMT("Chunks: %1", periods().size()).toStdString() << '\n';
        for (const auto& [name, r]: reports)
        {
            const Report& reference = reports[0].second;
            const auto print =
                // too many arguments for the macro
                NX_FMT("Algorithm: %1", name) +
                NX_FMT("\nBytes: %1, ref: %2(%3%)"
                       "\nElapsed, milliseconds:"
                       "\nmin: %4, ref: %5(%6%)"
                       "\nmax: %7, ref: %8(%9%)"
                       "\nmedian: %10, ref: %11(%12)%",
                    r.sizeBytes, reference.sizeBytes, diffPerc(reference.sizeBytes, r.sizeBytes),
                    r.min, reference.min, diffPerc(reference.min.count(), r.min.count()),
                    r.max, reference.max, diffPerc(reference.max.count(), r.max.count()),
                    r.median, reference.median, diffPerc(reference.median.count(), r.median.count())
                );
            std::cout << '\n' << print.toStdString() << '\n';
        }
    }

private:
    enum class Distribution
    {
        uniform,
        normal,
    };

    struct GenerateArgs
    {
        size_t n;
        std::chrono::milliseconds durationMin = std::chrono::milliseconds{200};
        std::chrono::milliseconds durationMax = std::chrono::hours{1};
        Distribution durationDistribution = Distribution::uniform;
        // TODO:
        // Parameters:
        // - start time
    };

    static std::vector<QnTimePeriod> generatePeriods(const GenerateArgs& args) noexcept
    {
        switch (args.durationDistribution)
        {
            case Distribution::normal:
                return generatePeriods<std::normal_distribution<double>>(args);
            case Distribution::uniform:
            default:
                return generatePeriods<std::uniform_int_distribution<uint64_t>>(args);
        }
    }

    template <typename DistributionT>
    static std::vector<QnTimePeriod> generatePeriods(const GenerateArgs& args) noexcept
    {
        std::mt19937_64 gen64{};
        DistributionT durationDistributionMs(args.durationMin.count(), args.durationMax.count());

        std::vector<QnTimePeriod> result;
        result.reserve(args.n);

        uint64_t startMs = 0; // todo: from 2004
        // generate duration
        uint64_t durationMs = durationDistributionMs(gen64);
        for (size_t i = 0; i < args.n; ++i)
        {
            result.emplace_back(startMs, durationMs);
            startMs = /*random gap*/ durationDistributionMs(gen64) + startMs + durationMs;
            durationMs = durationDistributionMs(gen64);
        }

        return result;
    }

private:
    std::vector<QnTimePeriod> _periods;
};

TEST_F(TimePeriodCompressionBenchmark, DISABLED_Compare)
{
    // todo: parameterize?
    const size_t sampleSize = 10;
    const auto bench =
        [this, sampleSize](const auto& b) { return run(sampleSize, b); };

    const auto legacy =
        [](const auto& periods)
        {
            QByteArray buffer;
            QnCompressedTimeWriter<QByteArray> inputStream(&buffer, false);
            QnCompressedTime::serialize(periods, &inputStream);
            return buffer;
        };

    const auto makeJsonTest =
        [](auto makeString, auto compress)
        {
            return [=](const auto& periods) { return compress(makeString(periods)); };
        };

    const auto usingStringStream =
        [](const auto& periods) -> std::string
        {
            const size_t kDigits64 = 20;
            const size_t estimatedSize = 2 /*[]*/
                + periods.size() /*,*/
                + kDigits64 /*first - from epoch*/
                + kDigits64 * 2 * periods.size();

            std::string buffer;
            buffer.reserve(estimatedSize);
            std::stringstream json(std::move(buffer));

            json << '[';
            if (!periods.empty())
            {
                json << periods.front().startTimeMs;
                json << ',' << periods.front().startTimeMs;
                json << ',' << periods.front().durationMs;
            }

            for (size_t i = 1; i < periods.size(); ++i)
            {
                const auto& current = periods[i];
                const auto& prev = periods[i - 1];
                json << ',' << (current.startTimeMs - prev.startTimeMs)
                     << ',' << current.durationMs;
            }
            json << ']';

            json.str().shrink_to_fit();
            return std::move(json).str();
        };

    const auto usingToChars =
        [](const auto& periods) -> std::string
        {
            const size_t kDigits64 = 20;
            const size_t estimatedSize = 2 /*[]*/
                + periods.size() /*,*/
                + kDigits64 /*first - from epoch*/
                + kDigits64 * 2 * periods.size();
            const auto writeNumber = [](auto num, std::string& b, size_t where)
            {
                auto r = std::to_chars(&b[where], &b[where + kDigits64], num);
                return std::distance(&b[0], r.ptr);
            };

            std::string buffer(estimatedSize, '\0');
            // buffer.reserve(estimateSize); // would be nice to not initialize the content, but the
            // logic becomes really convoluted, but the logic becomes really convoluted

            size_t written = 0;
            buffer[written++] = '[';
            if (!periods.empty())
            {
                // epoch
                written = writeNumber(periods.front().startTimeMs, buffer, written);
                buffer[written++] = ',';
                // first chunk with delta to "prev" (epoch - epoch = 0)
                written = writeNumber(0, buffer, written);
                buffer[written++] = ',';
                written = writeNumber(periods.front().durationMs, buffer, written);
            }

            for (size_t i = 1; i < periods.size(); ++i)
            {
                const auto& current = periods[i];
                const auto& prev = periods[i - 1];
                buffer[written++] = ',';
                written = writeNumber(current.startTimeMs - prev.startTimeMs, buffer, written);
                buffer[written++] = ',';
                written = writeNumber(current.durationMs, buffer, written);
            }

            buffer[written++] = ']';
            buffer.resize(written);
            buffer.shrink_to_fit();
            return buffer;
        };

    const auto usingRapidJson =
        [](const auto& periods)
        {
            const size_t kDigits64 = 20;
            const size_t estimatedSize = 2 /*[]*/
                + periods.size() /*,*/
                + kDigits64 /*first - from epoch*/
                + kDigits64 * 2 * periods.size();

            rapidjson::StringBuffer buffer{nullptr, estimatedSize};
            rapidjson::Writer writer(buffer);
            writer.StartArray();

            if (!periods.empty())
            {
                // epoch
                writer.Uint(periods.front().startTimeMs);
                // first chunk with delta to "prev" (epoch - epoch = 0)
                writer.Uint(0);
                writer.Uint(periods.front().durationMs);
            }

            for (size_t i = 1; i < periods.size(); ++i)
            {
                const auto& current = periods[i];
                const auto& prev = periods[i - 1];
                writer.Uint(current.startTimeMs - prev.startTimeMs);
                writer.Uint(current.durationMs);
            }

            writer.EndArray();
            buffer.ShrinkToFit();

            return std::string(buffer.GetString(), buffer.GetLength());
        };

    const auto qCompress =
        [](const auto& string)
        {
            return nx::utils::bstream::gzip::Compressor::compressData(string, false);
        };

    using namespace std::string_view_literals;

    std::array reports{std::pair{"Legacy"sv, bench(legacy)},
        std::pair{"std::stringstream + qCompress"sv,
            bench(makeJsonTest(usingStringStream, qCompress))},
        std::pair{"std::to_chars + qCompress"sv,
            bench(makeJsonTest(usingToChars, qCompress))},
        std::pair{"rapidjson + qCompress"sv,
            bench(makeJsonTest(usingRapidJson, qCompress))}};

    printReports(reports);
}

} // namespace
