// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "text_to_wave_server.h"

#include <flite.h>

extern "C" {

cst_voice* register_cmu_us_slt();
void unregister_cmu_us_slt(cst_voice* vox);

} // extern "C"

// Add unique_ptr type with custom deleter function.
template<auto F>
using DeleterFromFunc = std::integral_constant<decltype(F), F>;

template<typename T, auto F>
using CustomUniquePtr = std::unique_ptr<T, DeleterFromFunc<F>>;

namespace {

// TODO: #akolesnikov Use ffmpeg instead of the following code (taken from speech_tools).

static bool saveRawData(
    QIODevice* fp,
    const short* data,
    int offset,
    int sampleCount,
    int channelCount)
{
    qint64 n = 0;

    if (CST_BIG_ENDIAN)
    {
        CustomUniquePtr<short, cst_free> xdata(cst_alloc(short, channelCount * sampleCount));
        memmove(xdata.get(), data + (offset * channelCount),
            channelCount*sampleCount*sizeof(short));
        swap_bytes_short(xdata.get(), channelCount*sampleCount);
        //n = fwrite(xdata, sizeof(short),channelCount * sampleCount, fp);
        n = fp->write(
            (const char*) xdata.get(), sizeof(short) * channelCount * sampleCount
        ) / sizeof(short);
    }
    else
    {
        //n = fwrite(&data[offset], sizeof(short), channelCount * sampleCount, fp);
        n = fp->write(
            (const char*) &data[offset], sizeof(short) * channelCount * sampleCount
        ) / sizeof(short);
    }

    if (n != (channelCount * sampleCount))
        return false;

    return true;
}

static bool saveWaveRiff(
    QIODevice* fp,
    const short* data,
    int offset,
    int sampleCount,
    int channelCount,
    int sampleRate)
{
    const char* info;
    int dataSize, dataInt;
    short dataShort;

    const auto kWordSize = sizeof(short);

    info = "RIFF";
    //fwrite(info, 4, 1, fp);
    fp->write(info, 4);
    dataSize = channelCount * sampleCount * kWordSize + 8 + 16 + 12;
    // WAV files are always LITTLE_ENDIAN (i.e. intel x86 format).
    if (CST_BIG_ENDIAN)
        dataSize = SWAPINT(dataSize);
    //fwrite(&data_size, 1, 4, fp); //< total number of bytes in file
    fp->write((const char*) &dataSize, 4);
    info = "WAVE";
    //fwrite(info, 4, 1, fp);
    fp->write((const char*) info, 4);
    info = "fmt ";
    //fwrite(info, 4, 1, fp);
    fp->write((const char*) info, 4);
    dataSize = 16;
    if (CST_BIG_ENDIAN)
        dataSize = SWAPINT(dataSize);
    //fwrite(&data_size, 1, 4, fp); //< size of header
    fp->write((const char*) &dataSize, 4);
    dataShort = RIFF_FORMAT_PCM;
    if (CST_BIG_ENDIAN)
        dataShort = SWAPSHORT(dataShort);
    //fwrite(&dataShort, 1, 2, fp); //< sample type
    fp->write((const char*) &dataShort, 2);
    dataShort = channelCount;
    if (CST_BIG_ENDIAN)
        dataShort = SWAPSHORT(dataShort);
    //fwrite(&dataShort, 1, 2, fp); //< number of channels
    fp->write((const char*) &dataShort, 2);
    dataInt = sampleRate;
    if (CST_BIG_ENDIAN)
        dataInt = SWAPINT(dataInt);
    //fwrite(&dataInt, 1, 4, fp); //< sample rate
    fp->write((const char*) &dataInt, 4);
    dataInt = sampleRate * channelCount * kWordSize;
    if (CST_BIG_ENDIAN)
        dataInt = SWAPINT(dataInt);
    //fwrite(&dataInt, 1, 4, fp); //< Average bytes per second
    fp->write((const char*) &dataInt, 4);
    dataShort = channelCount * kWordSize;
    if (CST_BIG_ENDIAN)
        dataShort = SWAPSHORT(dataShort);
    //fwrite(&dataShort, 1, 2, fp); //< block align
    fp->write((const char*)&dataShort, 2);
    dataShort = kWordSize * 8;
    if (CST_BIG_ENDIAN)
        dataShort = SWAPSHORT(dataShort);
    //fwrite(&dataShort, 1, 2, fp); //< bits per sample
    fp->write((const char*) &dataShort, 2);
    info = "data";
    //fwrite(info, 4, 1, fp);
    fp->write(info, 4);
    dataSize = channelCount * sampleCount * kWordSize;
    if (CST_BIG_ENDIAN)
        dataSize = SWAPINT(dataSize);
    //fwrite(&dataSize, 1, 4, fp); //< total number of bytes in data
    fp->write((const char*) &dataSize, 4);

    return saveRawData(fp, data, offset, sampleCount, channelCount);
}

static bool textToWave(
    const QString& text,
    cst_voice* vox,
    QIODevice* const dest,
    nx::media::audio::Format* outFormat)
{
    CustomUniquePtr<cst_wave, delete_wave> wave(
        flite_text_to_wave(text.toLatin1().constData(), vox));

    if (outFormat)
    {
        outFormat->sampleRate = wave->sample_rate;
        outFormat->channelCount = wave->num_channels;

        // Flite generates audio in audio/pcm with signed 16-bit integers (short).
        outFormat->codec = "audio/pcm";
        outFormat->byteOrder = QSysInfo::LittleEndian;

        const int sampleSize = sizeof(short) * 8;

        outFormat->sampleSize = sampleSize;
        outFormat->sampleType = nx::media::audio::Format::SampleType::signedInt;
    }

    if (!wave)
        return false;

    return saveWaveRiff(
        dest, wave->samples, 0, wave->num_samples, wave->num_channels,
        wave->sample_rate);
}

/** Flite wrapper - only this class uses Flite directly via the functions above. */
class FliteEngine
{
public:
    FliteEngine(const QString& /*binaryPath*/, nx::utils::promise<void>& initializedPromise)
    {
        flite_init();

        vox.reset(register_cmu_us_slt()); //< US female voice.

        flite_feat_set_float(vox->features, "duration_stretch", 1.0); //< Speedup/slowdown.
        flite_feat_set_float(vox->features, "int_f0_target_mean", 180); //< Pitch.

        initializedPromise.set_value();
    };

    ~FliteEngine()
    {
    };

    /** NOTE: Not declared static to emphasize the need for the initialized instance. */
    bool textToWave(const QString& text, QIODevice* const dest, nx::media::audio::Format* outFormat) const
    {
        return ::textToWave(text, vox.get(), dest, outFormat);
    }

private:
    CustomUniquePtr<cst_voice, unregister_cmu_us_slt> vox; //< Flite voice instance.
};

} // namespace

//-------------------------------------------------------------------------------------------------

template<>
nx::speech_synthesizer::TextToWaveServer*
    Singleton<nx::speech_synthesizer::TextToWaveServer>::s_instance = nullptr;

namespace nx::speech_synthesizer {

TextToWaveServer::TextToWaveServer(const QString& binaryPath):
    m_binaryPath(binaryPath),
    m_prevTaskId(1)
{
    m_initializedFuture = m_initializedPromise.get_future();
}

TextToWaveServer::~TextToWaveServer()
{
    stop();
}

void TextToWaveServer::waitForStarted()
{
    m_initializedFuture.get();
}

void TextToWaveServer::pleaseStop()
{
    QnLongRunnable::pleaseStop();

    // TODO: Instead of pushing an empty marker, consider calling setTerminated(true).
    m_textQueue.push(QSharedPointer<SynthesizeSpeechTask>(new SynthesizeSpeechTask()));
}

int TextToWaveServer::generateSoundAsync(const QString& text, QIODevice* const dest)
{
    QSharedPointer<SynthesizeSpeechTask> task = addTaskToQueue(text, dest);
    return task ? task->id : 0;
}

bool TextToWaveServer::generateSoundSync(
    const QString& text,
    QIODevice* const dest,
    nx::media::audio::Format* outFormat)
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    const QSharedPointer<SynthesizeSpeechTask> task = addTaskToQueue(text, dest);
    while (!task->done)
        m_cond.wait(lock.mutex());

    if (outFormat)
        *outFormat = task->format;

    return task->result;
}

void TextToWaveServer::run()
{
    initSystemThreadId();

    FliteEngine fliteEngine(m_binaryPath, m_initializedPromise);

    while (!needToStop())
    {
        QSharedPointer<SynthesizeSpeechTask> task;
        if (!m_textQueue.pop(task))
            continue;
        if (!task->dest)
            continue;

        const bool result = fliteEngine.textToWave(task->text, task->dest, &task->format);

        {
            NX_MUTEX_LOCKER lock(&m_mutex);
            task->done = true;
            task->result = result;
            m_cond.wakeAll();
        }

        emit done(task->id, task->result);
    }
}

QSharedPointer<TextToWaveServer::SynthesizeSpeechTask> TextToWaveServer::addTaskToQueue(
    const QString& text, QIODevice* const dest)
{
    QSharedPointer<SynthesizeSpeechTask> task(new SynthesizeSpeechTask());
    int taskId = m_prevTaskId.fetchAndAddAcquire(1);
    if (taskId == 0)
        taskId = m_prevTaskId.fetchAndAddAcquire(1);
    task->id = taskId;
    task->text = text;
    task->dest = dest;
    return m_textQueue.push(task) ? task : QSharedPointer<SynthesizeSpeechTask>();
}

} // namespace nx::speech_synthesizer
