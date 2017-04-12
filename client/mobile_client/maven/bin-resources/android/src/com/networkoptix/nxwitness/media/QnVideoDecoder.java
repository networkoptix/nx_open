package com.networkoptix.nxwitness.media;
import android.view.View;
import android.view.Window;
import android.view.Display;
import android.view.WindowManager;
import android.view.WindowManager.LayoutParams;
import android.graphics.Color;
import android.content.res.Resources;
import android.content.res.Configuration;
import android.util.DisplayMetrics;
import android.app.Activity;
import android.os.Build;
import android.util.Log;
import android.annotation.TargetApi;
import android.media.MediaCodec;
import android.media.MediaFormat;
import android.view.Surface;
import java.nio.ByteBuffer;
import org.qtproject.qt5.android.QtNative;
import java.util.concurrent.Callable;
import android.graphics.SurfaceTexture;
import android.media.MediaCodecInfo;
import android.media.MediaCodecInfo.CodecCapabilities;
import android.media.MediaCodecInfo.VideoCapabilities;
import android.media.MediaCodecList;

public class QnVideoDecoder
{
    // ATTENTION: These constants are coupled with the ones in android_video_decoder.cpp.
    private static final long kNoInputBuffers = -7;
    private static final long kCodecFailed = -8;

    private static final String TAG = "QnVideoDecoder";

    private static void logV(String message)
    {
        Log.v(TAG, message);
    }

    private static void logD(String message)
    {
        Log.d(TAG, message);
    }

    /**
     * Call callable (in the same thread) and handle all exceptions.
     */
    private long runCatching(String logPrefix, Callable<Long> callable)
    {
        try
        {
            return callable.call();
        }
        catch (IllegalStateException e)
        {
            // Requires API level 21.
            if (android.os.Build.VERSION.SDK_INT >= android.os.Build.VERSION_CODES.LOLLIPOP)
            {
                if (e instanceof MediaCodec.CodecException)
                {
                    MediaCodec.CodecException ex = (MediaCodec.CodecException) e;

                    m_hasCodecFailed = true;

                    logD(logPrefix + "CodecException: " + e.toString());
                    logD(logPrefix + "CodecException message: " + e.getMessage());

                    if (ex.isTransient())
                    {
                        return -1; //< Attempt to decode further data.
                    }
                    else
                    {
                        // NOTE: Even if e.isRecoverable(), we still prefer to signal m_codec failure which
                        // will reinitialize this QnVideoDecoder.
                        return kCodecFailed;
                    }
                }
            }
            logD(logPrefix + "IllegalStateException: " + e.toString());
            logD(logPrefix + "IllegalStateException message: " + e.getMessage());
            return -1;
        }
        catch (Exception e)
        {
            logD(logPrefix + "Exception: " + e.toString());
            logD(logPrefix + "Exception message: " + e.getMessage());
            return -1;
        }
    }

    private boolean doInit(String codecName, int width, int height)
        throws Exception
    {
        if (width <= 0 || height <= 0)
        {
            throw new IllegalArgumentException(
                "Invalid frame size: (" + width + ", " + height + ")");
        }

        doReleaseDecoder();
        m_hasCodecFailed = true; //< Will be reset to false after creating the codec.
        m_gotOutputData = false;

        logD("codecName: " + codecName);
        logD("width: " + width);
        logD("height: " + height);

        m_format = MediaFormat.createVideoFormat(codecName, width, height);
        logD("m_format created");

        // "video/avc"  - H.264/AVC video
        // "video/hevc" - H.265/HEVC video
        m_codec = MediaCodec.createDecoderByType(codecName);
        m_hasCodecFailed = false;
        logD("m_codec created");

        m_surfaceTexture = new SurfaceTexture(0, false);
        logD("m_surface created");

        m_surface = new Surface(m_surfaceTexture);
        m_codec.configure(m_format, m_surface, null, 0);

        m_codec.start();
        m_inputBuffers = m_codec.getInputBuffers();
        logD("m_codec started");

        return true;
    }

    public boolean init(final String codecName, final int width, final int height)
    {
        long result = runCatching("QnVideoDecoder.init(): ",
            new Callable<Long>()
            {
                public Long call()
                    throws Exception
                {
                    if (doInit(codecName, width, height))
                        return (long) 0;
                    else
                        return (long) -1;
                }
            });

        if (result < 0)
        {
            releaseDecoder();
            return false;
        }

        return true;
    }

    private static MediaCodecInfo selectCodec(String mimeType)
    {
         int numCodecs = MediaCodecList.getCodecCount();
         for (int i = 0; i < numCodecs; ++i)
         {
             MediaCodecInfo codecInfo = MediaCodecList.getCodecInfoAt(i);

             if (codecInfo.isEncoder())
                 continue;

             String[] types = codecInfo.getSupportedTypes();
             for (int j = 0; j < types.length; ++j)
             {
                 if (types[j].equalsIgnoreCase(mimeType))
                     return codecInfo;
             }
         }
         return null;
    }

    /**
     * @return Maximum supported frame width, or a non-positive value on error.
     */
    public int maxDecoderWidth(final String mimeType)
    {
        // Requires API level 21.
        if (android.os.Build.VERSION.SDK_INT < android.os.Build.VERSION_CODES.LOLLIPOP)
        {
            if ("video/avc".equalsIgnoreCase(mimeType))
                return 1920;
            else
                return 1280;
        }

        return (int) runCatching("QnVideoDecoder.maxDecoderWidth(): ",
            new Callable<Long>()
            {
                public Long call()
                    throws Exception
                {
                    MediaCodecInfo codecInfo = selectCodec(mimeType);
                    CodecCapabilities caps = codecInfo.getCapabilitiesForType(mimeType);
                    VideoCapabilities vcaps = caps.getVideoCapabilities();
                    return (long) vcaps.getSupportedWidths().getUpper();
                }
            });
    }

    /**
     * @return Maximum supported frame height, or a non-positive value on error.
     */
    public int maxDecoderHeight(final String mimeType)
    {
        // Requires API level 21.
        if (android.os.Build.VERSION.SDK_INT < android.os.Build.VERSION_CODES.LOLLIPOP)
        {
            if ("video/avc".equalsIgnoreCase(mimeType))
                return 1080;
            else
                return 720;
        }

        return (int) runCatching("QnVideoDecoder.maxDecoderHeight(): ",
            new Callable<Long>()
            {
                public Long call()
                    throws Exception
                {
                    MediaCodecInfo codecInfo = selectCodec(mimeType);
                    CodecCapabilities caps = codecInfo.getCapabilitiesForType(mimeType);
                    VideoCapabilities vcaps = caps.getVideoCapabilities();
                    return (long) vcaps.getSupportedHeights().getUpper();
                }
            });
    }

    private long doDecodeFrame(long srcDataPtr, int frameSize, long frameNum)
        throws Exception
    {
        if (m_hasCodecFailed)
        {
            logV("decodeFrame() called on failed cocec.");
            return kCodecFailed;
        }

        final long timeoutUs = 1000 * 1000; //< 1 second

        int inputBufferId = m_codec.dequeueInputBuffer(timeoutUs);
        if (inputBufferId >= 0)
        {
            ByteBuffer inputBuffer = m_inputBuffers[inputBufferId];

            // C++ callback.
            fillInputBuffer(inputBuffer, srcDataPtr, frameSize, inputBuffer.capacity());

            m_codec.queueInputBuffer(inputBufferId, 0, frameSize, frameNum, 0);
        }
        else
        {
            logD("error dequeueInputBuffer");
            return kNoInputBuffers; //< No input buffers left.
        }

        for (;;)
        {
            MediaCodec.BufferInfo info = new MediaCodec.BufferInfo();
            int outputBufferId =
                m_codec.dequeueOutputBuffer(info, m_gotOutputData ? timeoutUs : 1000 * 10);
            logV("dequeue buffer result=" + outputBufferId);
            switch (outputBufferId)
            {
                case MediaCodec.INFO_OUTPUT_BUFFERS_CHANGED:
                    logV("MediaCodec.INFO_OUTPUT_BUFFERS_CHANGED");
                    break;
                case MediaCodec.INFO_OUTPUT_FORMAT_CHANGED:
                    m_format = m_codec.getOutputFormat(); // option B
                    logV("MediaCodec.INFO_OUTPUT_FORMAT_CHANGED");
                    m_gotOutputData = true;
                    break;
                case MediaCodec.INFO_TRY_AGAIN_LATER:
                    logV("MediaCodec.INFO_TRY_AGAIN_LATER");
                    return 0; //< no error
                default:
                    long outFrameNum = info.presentationTimeUs;
                    m_codec.releaseOutputBuffer(outputBufferId, /*render*/ true);
                    return outFrameNum;
            }
        }
    }

    /**
     * @return Either frame number of the decoded frame, or 0 if no frames left, or kCodecFailed,
     * or kNoInputBuffers, or a different negative value on other errors.
     */
    public long decodeFrame(final long srcDataPtr, final int frameSize, final long frameNum)
    {
        return runCatching("QnVideoDecoder.decodeFrame(): ",
            new Callable<Long>()
            {
                public Long call()
                    throws Exception
                {
                    return doDecodeFrame(srcDataPtr, frameSize, frameNum);
                }
            });
    }

    public long doFlushFrame(long timeoutUs)
        throws Exception
    {
        if (m_hasCodecFailed)
        {
            logV("flushFrame() called on failed cocec.");
            return kCodecFailed;
        }

        //m_codec.flush();
        logV("flushFrame(" + timeoutUs + ") BEGIN");
        MediaCodec.BufferInfo info = new MediaCodec.BufferInfo();
        int outputBufferId = m_codec.dequeueOutputBuffer(info, timeoutUs);
        switch (outputBufferId)
        {
            case MediaCodec.INFO_OUTPUT_BUFFERS_CHANGED:
            case MediaCodec.INFO_OUTPUT_FORMAT_CHANGED:
            case MediaCodec.INFO_TRY_AGAIN_LATER:
            logV("flushFrame(" + timeoutUs + ") END -> 0");
                return 0; // no more frames left
            default:
                long outFrameNum = info.presentationTimeUs;
                m_codec.releaseOutputBuffer(outputBufferId, /*render*/ true);
                logV("flushFrame(" + timeoutUs + ") END -> " + outFrameNum);
                return outFrameNum;
        }
    }

    /**
     * @return Either frame number of the flushed frame, or 0 if no frames left, or kCodecFailed.
     */
    public long flushFrame(final long timeoutUs)
    {
        return runCatching("QnVideoDecoder.flushFrame(): ",
            new Callable<Long>()
            {
                public Long call()
                    throws Exception
                {
                    return doFlushFrame(timeoutUs);
                }
            });
    }

    public void updateTexImage()
    {
        // Exceptions are logged and ignored.
        runCatching("QnVideoDecoder.updateTexImage(): ",
            new Callable<Long>()
            {
                public Long call()
                    throws Exception
                {
                    m_surfaceTexture.updateTexImage();
                    return (long) 0;
                }
            });
    }

    public void getTransformMatrix(final float[] mtx)
    {
        // Exceptions are logged and ignored.
        runCatching("QnVideoDecoder.updateTexImage(): ",
            new Callable<Long>()
            {
                public Long call()
                    throws Exception
                {
                    m_surfaceTexture.getTransformMatrix(mtx);
                    return (long) 0;
                }
            });
    }

    private void doReleaseDecoder()
        throws Exception
    {
        logD("releaseDecoder() BEGIN");

        if (m_codec != null)
        {
            if (!m_hasCodecFailed)
                m_codec.stop();
            logV("releaseDecoder(): m_codec.stop() finished, calling m_codec.release()...");
            m_codec.release();
            m_hasCodecFailed = false;
        }

        m_inputBuffers = null;
        m_codec = null;
        m_hasCodecFailed = false;
        m_format = null;
        m_surface = null;
        m_surfaceTexture = null;
        m_gotOutputData = false;

        logV("releaseDecoder() END");
    }

    public void releaseDecoder()
    {
        // Exceptions are logged and ignored.
        runCatching("QnVideoDecoder.releaseDecoder(): ",
            new Callable<Long>()
            {
                public Long call()
                    throws Exception
                {
                    doReleaseDecoder();
                    return (long) 0;
                }
            });
    }

    private static native void fillInputBuffer(
        ByteBuffer buffer, long srcDataPtr, int frameSize, int capacity);

    ByteBuffer[] m_inputBuffers;
    private MediaCodec m_codec;
    private boolean m_hasCodecFailed;
    private MediaFormat m_format;
    private Surface m_surface;
    private SurfaceTexture m_surfaceTexture;
    private boolean m_gotOutputData;
}
