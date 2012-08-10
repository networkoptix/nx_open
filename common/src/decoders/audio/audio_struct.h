#ifndef audio_struct_h1530
#define audio_struct_h1530

#ifndef Q_OS_WIN
#include "utils/media/audioformat.h"
#else
#include <QAudioFormat>
#define QnAudioFormat QAudioFormat
#endif

#endif //audio_struct_h1530
