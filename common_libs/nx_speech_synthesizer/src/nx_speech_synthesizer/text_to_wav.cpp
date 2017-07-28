#if !defined(EDGE_SERVER) && !defined(DISABLE_FESTIVAL)
#include "text_to_wav.h"

#include <QtCore/QFile>

#include <mutex>
#include <memory>

#include <festival/festival.h>
#include <EST_wave_aux.h>

#ifdef QN_USE_VLD
#include <vld.h>
#endif

static char festivalVoxPath[256];

#if (_XOPEN_SOURCE >= 700 || _POSIX_C_SOURCE >= 200809L) && 0
#include <stdio.h>
#else
namespace
{
    //TODO/IMPL: #ak use ffmpeg instead of following code (stolen from speech_tools)

#ifndef WAVE_FORMAT_PCM
    #define WAVE_FORMAT_PCM    0x0001
#endif
    #define WAVE_FORMAT_ADPCM  0x0002
    #define WAVE_FORMAT_ALAW   0x0006
    #define WAVE_FORMAT_MULAW  0x0007

    void short_to_schar(const short *data,unsigned char *chars,int length)
    {
        /* Convert shorts to 8 bit SIGNED CHAR */
        int i;

        for (i=0; i<length; i++)
        chars[i] = (data[i]/256);

    }

    void short_to_uchar(const short *data,unsigned char *chars,int length)
    {
        /* Convert shorts to 8 bit UNSIGNED CHAR */
        int i;

        for (i=0; i<length; i++)
        chars[i] = (data[i]/256)+128;

    }

    int get_word_size(enum EST_sample_type_t sample_type)
    {
        int word_size = 0;

        switch (sample_type)
        {
            case st_unknown:
                word_size = 2; break;
            case st_uchar:
            case st_schar:
                word_size = 1; break;
            case st_mulaw:
                word_size = 1; break;
            case st_short:
                word_size = 2; break;
            case st_int:
                /* Yes I mean 4 not sizeof(int) these are machine independent defs */
                word_size = 4; break;
            case st_float:
                word_size = 4; break;
            case st_double:
                word_size = 8; break;
            default:
                NX_ASSERT(false, "TextToWaveServer, unknown sample size.");
        }

        return word_size;
    }

    EST_sample_type_t fromStringToEstSampleType(const EST_String& sampleTypeStr)
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
        if (sampleTypeStr.matches("real") || sampleTypeStr.matches("float") || sampleTypeStr.matches("real4"))
            return st_float;
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

    enum EST_write_status save_raw_data(QIODevice *fp, const short *data, int offset,
                        int num_samples, int num_channels,
                        const enum EST_sample_type_t sample_type,
                        int bo)
    {
        //int i;
        int n;
        if (sample_type == st_short)
        {
            if (bo != EST_NATIVE_BO)
            {
                short *xdata = walloc(short,num_channels*num_samples);
                memmove(xdata,data+(offset*num_channels),
                    num_channels*num_samples*sizeof(short));
                swap_bytes_short(xdata,num_channels*num_samples);
                //n = fwrite(xdata, sizeof(short),num_channels * num_samples, fp);
                n = fp->write( (const char*)xdata, sizeof(short) * num_channels * num_samples ) / sizeof(short);
                wfree(xdata);
            }
            else
            {
                //n = fwrite(&data[offset], sizeof(short), num_channels * num_samples, fp);
                n = fp->write( (const char*)&data[offset], sizeof(short) * num_channels * num_samples ) / sizeof(short);
            }
            if (n != (num_channels * num_samples))
                return misc_write_error;
        }
        else
        {
            fprintf(stderr,"save data file: unsupported sample type\n");
            return misc_write_error;
        }
        return write_ok;
    }


    enum EST_write_status save_wave_riff(QIODevice *fp, const short *data, int offset,
                         int num_samples, int num_channels,
                         int sample_rate,
                         const enum EST_sample_type_t sample_type, int bo)
    {

        (void)bo;
        const char *info;
        int data_size, data_int;
        short data_short;

        NX_ASSERT(sample_type != st_schar, "TextToWaveServer, RIFF format: Signed 8-bit not allowed by this file format");

        info = "RIFF";
        //fwrite(info,4,1,fp);
        fp->write( info, 4 );
        data_size = num_channels*num_samples*get_word_size(sample_type)+ 8+16+12;
        /* WAV files are always LITTLE_ENDIAN (i.e. intel x86 format) */
        if (EST_BIG_ENDIAN) data_size = SWAPINT(data_size);
        //fwrite(&data_size,1,4,fp); /* total number of bytes in file */
        fp->write( (const char*)&data_size, 4 );
        info = "WAVE";
        //fwrite(info,4,1,fp);
        fp->write( (const char*)info, 4 );
        info = "fmt ";
        //fwrite(info,4,1,fp);
        fp->write( (const char*)info, 4 );
        data_size = 16;
        if (EST_BIG_ENDIAN) data_size = SWAPINT(data_size);
        //fwrite(&data_size,1,4,fp); /* size of header */
        fp->write( (const char*)&data_size, 4 );
        switch (sample_type)
        {
            case st_short:  data_short = WAVE_FORMAT_PCM; break;
            case st_uchar: data_short = WAVE_FORMAT_PCM; break;
            case st_mulaw: data_short = WAVE_FORMAT_MULAW; break;
            case st_alaw: data_short = WAVE_FORMAT_ALAW; break;
            case st_adpcm: data_short = WAVE_FORMAT_ADPCM; break;
            default:
              fprintf(stderr,"RIFF format: unsupported data format %d\n",
                  sample_type);
              return misc_write_error;
        }
        if (EST_BIG_ENDIAN) data_short = SWAPSHORT(data_short);
        //fwrite(&data_short,1,2,fp); /* sample type */
        fp->write( (const char*)&data_short, 2 );
        data_short = num_channels;
        if (EST_BIG_ENDIAN) data_short = SWAPSHORT(data_short);
        //fwrite(&data_short,1,2,fp); /* number of channels */
        fp->write( (const char*)&data_short, 2 );
        data_int = sample_rate;
        if (EST_BIG_ENDIAN) data_int = SWAPINT(data_int);
        //fwrite(&data_int,1,4,fp); /* sample rate */
        fp->write( (const char*)&data_int, 4 );
        data_int = sample_rate * num_channels * get_word_size(sample_type);
        if (EST_BIG_ENDIAN) data_int = SWAPINT(data_int);
        //fwrite(&data_int,1,4,fp); /* Average bytes per second */
        fp->write( (const char*)&data_int, 4 );
        data_short = num_channels * get_word_size(sample_type);
        if (EST_BIG_ENDIAN) data_short = SWAPSHORT(data_short);
        //fwrite(&data_short,1,2,fp); /* block align */
        fp->write( (const char*)&data_short, 2 );
        data_short = get_word_size(sample_type) * 8;
        if (EST_BIG_ENDIAN) data_short = SWAPSHORT(data_short);
        //fwrite(&data_short,1,2,fp); /* bits per sample */
        fp->write( (const char*)&data_short, 2 );
        info = "data";
        //fwrite(info,4,1,fp);
        fp->write( info, 4 );
        data_size = num_channels*num_samples*get_word_size(sample_type);
        if (EST_BIG_ENDIAN) data_size = SWAPINT(data_size);
        //fwrite(&data_size,1,4,fp); /* total number of bytes in data */
        fp->write( (const char*)&data_size, 4 );

        return save_raw_data(fp,data,offset,num_samples,num_channels,
                 sample_type,bo_little);
    }
}
#endif

static void initFestival(const QString binaryPath)
{
#ifdef QN_USE_VLD
    VLDDisable();
#endif
    //initializing festival engine
    //sprintf( festivalVoxPath, "%s/festival.vox/lib/", QN_BUILDENV_PATH );
#ifndef Q_OS_MAC
    sprintf( festivalVoxPath, "%s/vox/", binaryPath.toLatin1().constData() );
#else
    sprintf( festivalVoxPath, "%s/../Resources/vox/", binaryPath.toLatin1().constData() );
#endif
    festival_libdir = festivalVoxPath;

    const int heap_size = 1510000;  // default scheme heap size
    const int load_init_files = 1; // we want the festival init files loaded
    festival_initialize( load_init_files, heap_size );
#ifdef QN_USE_VLD
    VLDEnable();
#endif
}

static void deinitFestival()
{
    festival_wait_for_spooler();
    festival_tidy_up();
}

static int my_festival_text_to_wave(const EST_String &text,EST_Wave &wave)
{
    return festival_text_to_wave( text, wave );
}

static bool textToWavInternal(const QString& text, QIODevice* const dest, QnAudioFormat* outFormat)
{
    // Convert to a waveform
    EST_Wave wave;
    EST_String srcText( text.toLatin1().constData() );
    bool result = my_festival_text_to_wave( srcText, wave );

    if (outFormat)
    {
        outFormat->setSampleRate(wave.sample_rate());
        outFormat->setChannelCount(wave.num_channels());

        // TODO: #dmishin set format properties according to EST_Wave's sample_type info.
        outFormat->setCodec(QString("audio/pcm"));
        outFormat->setByteOrder(QnAudioFormat::LittleEndian);

        auto sampleType = fromStringToEstSampleType(wave.sample_type());

        NX_ASSERT(sampleType != st_unknown, lm("TextToWaveServer, unknown sample format."));

        auto sampleSize = get_word_size(sampleType) * 8;

        NX_ASSERT(sampleSize, lm("TextToWaveServer, unknown sample size"));

        outFormat->setSampleSize(sampleSize);
        outFormat->setSampleType(fromEstSampleTypeToQt(sampleType));
    }

    if( result )
    {
#if (_XOPEN_SOURCE >= 700 || _POSIX_C_SOURCE >= 200809L) && 0
        //open_memstream is present
        char* buf = NULL;
        size_t bufSize = 0;
        FILE* f = open_memstream( &buf, &bufSize );
        if( !f )
            return false;
        if( wave.save( f, "riff" ) != write_ok )
        {
            if( buf )
                free( buf );
            result = false;
        }
        else
        {
            result = (buf && (bufSize > 0))
                ? dest->write( buf, bufSize ) == (qint64)bufSize
                : false;
            if( buf )
                free( buf );
        }
        fflush( f );
        fclose( f );
#else
        result = save_wave_riff( dest, wave.values().memory(), 0, wave.num_samples(), wave.num_channels(), wave.sample_rate(), st_short, EST_NATIVE_BO ) == write_ok;
#endif
    }

    return result;
}

class FestivalInitializer
{

public:
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


TextToWaveServer::TextToWaveServer(const QString& binaryPath):
    m_binaryPath(binaryPath),
    m_prevTaskID(1)
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
    m_textQueue.push( QSharedPointer<SynthetiseSpeechTask>(new SynthetiseSpeechTask()) );
}

int TextToWaveServer::generateSoundAsync( const QString& text, QIODevice* const dest )
{
    QSharedPointer<SynthetiseSpeechTask> task = addTaskToQueue( text, dest );
    return task ? task->id : 0;
}

bool TextToWaveServer::generateSoundSync(
    const QString& text,
    QIODevice* const dest,
    QnAudioFormat* outFormat)
{
    QnMutexLocker lk( &m_mutex );
    QSharedPointer<SynthetiseSpeechTask> task = addTaskToQueue( text, dest );
    while( !task->done )
        m_cond.wait( lk.mutex() );

    if (outFormat)
        *outFormat = task->format;

    return task->result;
}

void TextToWaveServer::run()
{
    initSystemThreadId();

    FestivalInitializer(m_binaryPath, m_initializedPromise);

    while( !needToStop() )
    {
        QSharedPointer<SynthetiseSpeechTask> task;
        if( !m_textQueue.pop( task ) )
            continue;
        if( !task->dest )
            continue;

        const bool result = textToWavInternal(task->text, task->dest, &task->format);
        {
            QnMutexLocker lk( &m_mutex );
            task->done = true;
            task->result = result;
            m_cond.wakeAll();
        }
        emit done( task->id, task->result );
    }
}

QSharedPointer<TextToWaveServer::SynthetiseSpeechTask> TextToWaveServer::addTaskToQueue( const QString& text, QIODevice* const dest )
{
    QSharedPointer<SynthetiseSpeechTask> task( new SynthetiseSpeechTask() );
    int taskID = m_prevTaskID.fetchAndAddAcquire(1);
    if( taskID == 0 )
        taskID = m_prevTaskID.fetchAndAddAcquire(1);
    task->id = taskID;
    task->text = text;
    task->dest = dest;
    return m_textQueue.push( task ) ? task : QSharedPointer<SynthetiseSpeechTask>();
}
#endif
