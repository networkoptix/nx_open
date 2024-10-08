// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <vector>

#include <math.h>
#include <stdint.h>

#include <gtest/gtest.h>

extern "C" {
#include <libavcodec/avfft.h> //< for struct FFTComplex
} // extern "C"

#include <nx/kit/debug.h>
#include <nx/vms/client/core/media/voice_spectrum_analyzer.h>

namespace nx::vms::client::core {

static const int kChannelCount = 2;
static const int kSampleRate = 44100;
static const int kSampleCount = 1152; //< Number of int audio samples in one input portion.
static const int kBandCount = 15; //< Number of values in the spectrum.
static const int kWindowSize = 2048;

#define ARRAY_LEN(ARRAY) (sizeof(ARRAY) / sizeof((ARRAY)[0]))

static const int16_t kSamples1[] = {
#include "samples_1.inc"
};
static_assert(kSampleCount * kChannelCount == ARRAY_LEN(kSamples1));

static const int16_t kSamples2[] = {
#include "samples_2.inc"
};
static_assert(kSampleCount * kChannelCount == ARRAY_LEN(kSamples2));

static const double kExpectedSpectrum[] = {
#include "expected_spectrum.inc"
};
static_assert(kBandCount == ARRAY_LEN(kExpectedSpectrum));

static const FFTComplex kFftInput[] = {
#include "fft_input.inc"
};
static_assert(kWindowSize == ARRAY_LEN(kFftInput));

static const FFTComplex kExpectedFftOutput[] = {
#include "expected_fft_output.inc"
};
static_assert(kWindowSize == ARRAY_LEN(kExpectedFftOutput));

static void assertDoubleArraysEqual(
    const char tag[], const double expected[], const double actual[], int size)
{
    static const double kEpsilon = 0.0001;

    for (int i = 0; i < size; ++i)
    {
        ASSERT_TRUE(
            abs(expected[i] - actual[i]) < kEpsilon
        ) << "Comparing " << tag << " at index " << i << ": expected " << expected[i]
            << ", actual " << actual[i];
    }
}

static void assertComplexArraysEqual(
    const char tag[], const FFTComplex expected[], const FFTComplex actual[], int size)
{
    static const double kEpsilon = 0.0001;

    for (int i = 0; i < size; ++i)
    {
        ASSERT_TRUE(
            abs(expected[i].re - actual[i].re) < kEpsilon
        ) << "Comparing " << tag << " Re[" << i << "]: expected " << expected[i].re
            << ", actual " << actual[i].re;

        ASSERT_TRUE(
            abs(expected[i].im - actual[i].im) < kEpsilon
        ) << "Comparing " << tag << " Im[" << i << "]: expected " << expected[i].im
            << ", actual " << actual[i].im;
    }
}

// Inheriting to access protected members.
class TestVoiceSpectrumAnalyzer: public VoiceSpectrumAnalyzer
{
public:
    FFTComplex* access_fftData() { return fftData(); }
    int access_windowSize() { return windowSize(); }
    void access_performFft() { return performFft(); }
    std::string access_dump() { return dump(); }

    static SpectrumData access_fillSpectrumData(
        const FFTComplex data[], int size, int srcSampleRate)
    {
        return fillSpectrumData(data, size, srcSampleRate);
    }
};

static void assertSpectrumEquals(
    const char tag[], const double expected[], const SpectrumData& actual)
{
    ASSERT_EQ(kBandCount, actual.data.size());

    ASSERT_NO_FATAL_FAILURE(assertDoubleArraysEqual(
        nx::kit::utils::format("Spectrum %s", tag).c_str(),
        expected, actual.data.data(), kBandCount));
}

TEST(SpectrumAnalyzerTest, Fft)
{
    TestVoiceSpectrumAnalyzer analyzer;
    analyzer.initialize(kSampleRate, kChannelCount);

    ASSERT_EQ(analyzer.access_windowSize(), ARRAY_LEN(kFftInput));
    memcpy(analyzer.access_fftData(), kFftInput, sizeof(kFftInput));

    analyzer.access_performFft();

    ASSERT_NO_FATAL_FAILURE(assertComplexArraysEqual("FFT output",
        kExpectedFftOutput, analyzer.access_fftData(), ARRAY_LEN(kExpectedFftOutput)));
}

TEST(SpectrumAnalyzerTest, Spectrum)
{
    TestVoiceSpectrumAnalyzer analyzer;
    analyzer.initialize(kSampleRate, kChannelCount);

    std::vector<FFTComplex> fftData{kFftInput, kFftInput + ARRAY_LEN(kFftInput)};

    const SpectrumData spectrumData =
        analyzer.access_fillSpectrumData(kExpectedFftOutput, kWindowSize, kSampleRate);

    ASSERT_NO_FATAL_FAILURE(assertSpectrumEquals("for expected FFT output",
        kExpectedSpectrum, spectrumData));
}

TEST(SpectrumAnalyzerTest, Analyzer)
{
    TestVoiceSpectrumAnalyzer analyzer;
    analyzer.initialize(kSampleRate, kChannelCount);

    // Having the constant values as in this test, the Analyzer is expected to produce a spectrum
    // after processing two input sample portions.

    analyzer.processData(kSamples1, kSampleCount);

    ASSERT_EQ(0, analyzer.getSpectrumData().data.size()); //< Not enough input samples yet.

    analyzer.processData(kSamples2, kSampleCount);

    ASSERT_NO_FATAL_FAILURE(assertSpectrumEquals("for samples 1 and 2",
        kExpectedSpectrum, analyzer.getSpectrumData()));
}

} // namespace nx::vms::client::core
