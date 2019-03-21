#include "text_to_wave_server.h"

#if defined(QN_USE_VLD)
    #include <vld.h>
#endif

static char festivalVoxPath[256];

#include <festival/festival.h>
#include <EST_wave_aux.h>

namespace nx::speech_synthesizer {

namespace {

// TODO: #akolesnikov Use ffmpeg instead of the following code (taken from speech_tools).

#ifndef WAVE_FORMAT_PCM
    #define WAVE_FORMAT_PCM 0x0001
#endif
#define WAVE_FORMAT_ADPCM 0x0002
#define WAVE_FORMAT_ALAW 0x0006
#define WAVE_FORMAT_MULAW 0x0007

static int getWordSize(enum EST_sample_type_t sampleType)
{
    int wordSize = 0;

    switch (sampleType)
    {
        case st_unknown:
            wordSize = 2;
            break;
        case st_uchar:
        case st_schar:
            wordSize = 1;
            break;
        case st_mulaw:
            wordSize = 1;
            break;
        case st_short:
            wordSize = 2;
            break;
            // Here it is 4, not sizeof(int) - these are machine-independent definitions.
        case st_int:
            wordSize = 4;
            break;
        case st_float:
            wordSize = 4;
            break;
        case st_double:
            wordSize = 8;
            break;
        default:
            NX_ASSERT(false, "TextToWaveServer: Unknown sample size.");
    }

    return wordSize;
}

static EST_sample_type_t fromStringToEstSampleType(const EST_String& sampleTypeStr)
{
    if (sampleTypeStr.matches("short"))
        return st_short;
    if (sampleTypeStr.matches("shorten"))
        return st_shorten;
    if (sampleTypeStr.matches("ulaw") || sampleTypeStr.matches("mulaw"))
        return st_mulaw;

    if (sampleTypeStr.matches("char")
        || sampleTypeStr.matches("byte")
        || sampleTypeStr.matches("8bit"))
    {
        return st_schar;
    }

    if (sampleTypeStr.matches("unsignedchar")
        || sampleTypeStr.matches("unsignedbyte")
        || sampleTypeStr.matches("unsigned8bit"))
    {
        return st_uchar;
    }

    if (sampleTypeStr.matches("int"))
        return st_int;
    if (sampleTypeStr.matches("real") || sampleTypeStr.matches("float")
        || sampleTypeStr.matches("real4"))
    {
        return st_float;
    }
    if (sampleTypeStr.matches("real8") || sampleTypeStr.matches("double"))
        return st_double;
    if (sampleTypeStr.matches("alaw"))
        return st_alaw;
    if (sampleTypeStr.matches("ascii"))
        return st_ascii;

    return st_unknown;
}

QnAudioFormat::SampleType fromEstSampleTypeToQt(const EST_sample_type_t estSampleType)
{
    switch (estSampleType)
    {
        case st_short:
        case st_shorten:
        case st_schar:
        case st_int:
            return QnAudioFormat::SignedInt;
        case st_uchar:
            return QnAudioFormat::UnSignedInt;
        case st_float:
        case st_double:
            return QnAudioFormat::Float;
        default:
            return QnAudioFormat::Unknown;
    }
}

static enum EST_write_status saveRawData(
    QIODevice* fp,
    const short* data,
    int offset,
    int sampleCount,
    int channelCount,
    const enum EST_sample_type_t sampleType,
    int bo)
{
    int n;
    if (sampleType == st_short)
    {
        if (bo != EST_NATIVE_BO)
        {
            short *xdata = walloc(short,channelCount*sampleCount);
            memmove(xdata,data+(offset*channelCount),
                channelCount*sampleCount*sizeof(short));
            swap_bytes_short(xdata,channelCount*sampleCount);
            //n = fwrite(xdata, sizeof(short),channelCount * sampleCount, fp);
            n = (int) fp->write(
                (const char*) xdata, sizeof(short) * channelCount * sampleCount
            ) / sizeof(short);
            wfree(xdata);
        }
        else
        {
            //n = fwrite(&data[offset], sizeof(short), channelCount * sampleCount, fp);
            n = (int) fp->write(
                (const char*) &data[offset], sizeof(short) * channelCount * sampleCount
            ) / sizeof(short);
        }
        if (n != (channelCount * sampleCount))
            return write_error;
    }
    else
    {
        fprintf(stderr,"save data file: unsupported sample type\n");
        return write_error;
    }
    return write_ok;
}

static enum EST_write_status saveWaveRiff(
    QIODevice* fp,
    const short* data,
    int offset,
    int sampleCount,
    int channelCount,
    int sampleRate,
    const enum EST_sample_type_t sampleType,
    int /*bo*/)
{
    const char* info;
    int dataSize, dataInt;
    short dataShort;

    NX_ASSERT(sampleType != st_schar,
        "TextToWaveServer, RIFF format: Signed 8-bit not allowed by this file format.");

    info = "RIFF";
    //fwrite(info, 4, 1, fp);
    fp->write(info, 4);
    dataSize = channelCount * sampleCount * getWordSize(sampleType) + 8 + 16 + 12;
    // WAV files are always LITTLE_ENDIAN (i.e. intel x86 format).
    if (EST_BIG_ENDIAN)
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
    if (EST_BIG_ENDIAN)
        dataSize = SWAPINT(dataSize);
    //fwrite(&data_size, 1, 4, fp); //< size of header
    fp->write((const char*) &dataSize, 4);
    switch (sampleType)
    {
        case st_short: dataShort = WAVE_FORMAT_PCM; break;
        case st_uchar: dataShort = WAVE_FORMAT_PCM; break;
        case st_mulaw: dataShort = WAVE_FORMAT_MULAW; break;
        case st_alaw: dataShort = WAVE_FORMAT_ALAW; break;
        case st_adpcm: dataShort = WAVE_FORMAT_ADPCM; break;
        default:
            NX_ASSERT(false, lm("TextToWaveServer, RIFF format: Unsupported data format %1")
                .arg(sampleType));
            return write_error;
    }
    if (EST_BIG_ENDIAN)
        dataShort = SWAPSHORT(dataShort);
    //fwrite(&dataShort, 1, 2, fp); //< sample type
    fp->write((const char*) &dataShort, 2);
    dataShort = channelCount;
    if (EST_BIG_ENDIAN)
        dataShort = SWAPSHORT(dataShort);
    //fwrite(&dataShort, 1, 2, fp); //< number of channels
    fp->write((const char*) &dataShort, 2);
    dataInt = sampleRate;
    if (EST_BIG_ENDIAN)
        dataInt = SWAPINT(dataInt);
    //fwrite(&dataInt, 1, 4, fp); //< sample rate
    fp->write((const char*) &dataInt, 4);
    dataInt = sampleRate * channelCount * getWordSize(sampleType);
    if (EST_BIG_ENDIAN)
        dataInt = SWAPINT(dataInt);
    //fwrite(&dataInt, 1, 4, fp); //< Average bytes per second
    fp->write((const char*) &dataInt, 4);
    dataShort = channelCount * getWordSize(sampleType);
    if (EST_BIG_ENDIAN)
        dataShort = SWAPSHORT(dataShort);
    //fwrite(&dataShort, 1, 2, fp); //< block align
    fp->write((const char*)&dataShort, 2);
    dataShort = getWordSize(sampleType) * 8;
    if (EST_BIG_ENDIAN)
        dataShort = SWAPSHORT(dataShort);
    //fwrite(&dataShort, 1, 2, fp); //< bits per sample
    fp->write((const char*) &dataShort, 2);
    info = "data";
    //fwrite(info, 4, 1, fp);
    fp->write(info, 4);
    dataSize = channelCount * sampleCount * getWordSize(sampleType);
    if (EST_BIG_ENDIAN)
        dataSize = SWAPINT(dataSize);
    //fwrite(&dataSize, 1, 4, fp); //< total number of bytes in data
    fp->write((const char*) &dataSize, 4);

    return saveRawData(fp, data, offset, sampleCount, channelCount, sampleType, bo_little);
}

static void initFestival(const QString& binaryPath)
{
    #if defined(QN_USE_VLD)
        VLDDisable();
    #endif

    // Initializing festival engine.

    //sprintf(festivalVoxPath, "%s/festival.vox/lib/", QN_BUILDENV_PATH);
    #if !defined(Q_OS_MAC)
        sprintf(festivalVoxPath, "%s/vox/", binaryPath.toLatin1().constData());
    #else
        sprintf(festivalVoxPath, "%s/../Resources/vox/", binaryPath.toLatin1().constData());
    #endif

    festival_libdir = festivalVoxPath;

    const int kHeapSize = 1510000; //< default scheme heap size
    const int kLoadInitFiles = 1; //< we want the festival init files loaded
    festival_initialize(kLoadInitFiles, kHeapSize);

    #if defined(QN_USE_VLD)
        VLDEnable();
    #endif
}

static void deinitFestival()
{
    festival_wait_for_spooler();
    festival_tidy_up();
}

static int textToWaveFestival(const EST_String& text, EST_Wave& wave)
{
    return festival_text_to_wave(text, wave);
}

static bool textToWaveInternal(
    const QString& text, QIODevice* const dest, QnAudioFormat* outFormat)
{
    // Convert to a waveform.
    EST_Wave wave;
    const EST_String srcText(text.toLatin1().constData());
    const bool textToWaveFestivalSucceeded = textToWaveFestival(srcText, wave);

    if (outFormat)
    {
        outFormat->setSampleRate(wave.sample_rate());
        outFormat->setChannelCount(wave.num_channels());

        // TODO: #dmishin Set format properties according to EST_Wave's sample_type info.
        outFormat->setCodec(QString("audio/pcm"));
        outFormat->setByteOrder(QnAudioFormat::LittleEndian);

        const auto sampleType = fromStringToEstSampleType(wave.sample_type());

        NX_ASSERT(sampleType != st_unknown, "TextToWaveServer: Unknown sample format.");

        const int sampleSize = getWordSize(sampleType) * 8;

        NX_ASSERT(sampleSize, "TextToWaveServer: Unknown sample size.");

        outFormat->setSampleSize(sampleSize);
        outFormat->setSampleType(fromEstSampleTypeToQt(sampleType));
    }

    if (!textToWaveFestivalSucceeded)
        return false;

    return saveWaveRiff(
        dest, wave.values().memory(), 0, wave.num_samples(), wave.num_channels(),
        wave.sample_rate(), st_short, EST_NATIVE_BO
    ) == write_ok;
}

struct FestivalInitializer
{
    FestivalInitializer(const QString& binaryPath, nx::utils::promise<void>& initializedPromise)
    {
        initFestival(binaryPath);
        initializedPromise.set_value();
    };

    ~FestivalInitializer()
    {
        deinitFestival();
    };
};

} // namespace

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
    QnAudioFormat* outFormat)
{
    QnMutexLocker lock(&m_mutex);
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

    FestivalInitializer festivalInitializer(m_binaryPath, m_initializedPromise);

    while (!needToStop())
    {
        QSharedPointer<SynthesizeSpeechTask> task;
        if (!m_textQueue.pop(task))
            continue;
        if (!task->dest)
            continue;

        const bool result = textToWaveInternal(task->text, task->dest, &task->format);

        {
            QnMutexLocker lock(&m_mutex);
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
