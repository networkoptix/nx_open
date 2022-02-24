// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QIODevice>
#include <QtCore/QSharedPointer>
#include <QtCore/QString>

#include <nx/audio/format.h>

#include <nx/utils/thread/mutex.h>
#include <nx/utils/thread/wait_condition.h>
#include <nx/utils/singleton.h>
#include <nx/utils/std/future.h>
#include <nx/utils/thread/long_runnable.h>
#include <utils/common/threadqueue.h>

namespace nx::speech_synthesizer {

/**
 * Synthesizes wav based on a text. Uses Flite engine. Has an internal thread. Holds the queue
 * of texts to synthesize.
 *
 * Can be disabled (becomes a stub) via -DDISABLE_FLITE for the .cpp corresponding to this .h.
 */
class TextToWaveServer:
    public QnLongRunnable,
    public Singleton<TextToWaveServer>
{
    Q_OBJECT

public:
    /** Whether the Flite was disabled via -DDISABLE_FLITE. */
    static bool isEnabled();

    TextToWaveServer(const QString& binaryPath);
    virtual ~TextToWaveServer();

    void waitForStarted();

    virtual void pleaseStop() override;

public slots:
    /**
     * Adds the task to the queue.
     * @return Id of the generation context, or 0 in case of error.
     */
    int generateSoundAsync(const QString& text, QIODevice* const dest);

    /**
     * Adds the task to the queue and blocks until the speech is generated (or it fails).
     *
     * NOTE: This method is synchronous and reentrant.
     *
     * @param text Only latin-1 string is supported.
     * @return Generation result.
     */
    bool generateSoundSync(
        const QString& text,
        QIODevice* const dest,
        nx::audio::Format* outFormat = nullptr);

signals:
    /** Emitted in any case on text generation done (successful or not). */
    void done(int textId, bool result);

protected:
    virtual void run() override;

private:
    struct SynthesizeSpeechTask
    {
        int id = 0;
        QString text;
        QIODevice* dest = nullptr;
        nx::audio::Format format;
        bool result = false;
        bool done = false;
    };

    QString m_binaryPath;
    QnSafeQueue<QSharedPointer<SynthesizeSpeechTask>> m_textQueue;
    QAtomicInt m_prevTaskId;
    nx::WaitCondition m_cond;
    nx::Mutex m_mutex;

    nx::utils::promise<void> m_initializedPromise;
    nx::utils::future<void> m_initializedFuture;

    QSharedPointer<SynthesizeSpeechTask> addTaskToQueue(const QString& text, QIODevice* dest);
};

} // namespace nx::speech_synthesizer
