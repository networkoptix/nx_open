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
import android.graphics.SurfaceTexture;
import android.media.MediaCodecInfo;
import android.media.MediaCodecInfo.CodecCapabilities;
import android.media.MediaCodecInfo.VideoCapabilities;
import android.media.MediaCodecList;

public class QnVideoDecoder
{
    // ATTENTION: These constants are coupled with the ones in android_video_decoder.cpp.
    private static final int kNoInputBuffers = -7;
    private static final int kCodecFailed = -8;

    private static final String TAG = "QnVideoDecoder";

    public boolean init(String codecName, int width, int height)
    {
        try
        {
            releaseDecoder();
            m_hasCodecFailed = true; //< Will be reset to false after creating the codec.
            m_gotOutputData = false;

            Log.i(TAG, "codecName: " + codecName);
            Log.i(TAG, "width: " + width);
            Log.i(TAG, "height: " + height);

            m_format = MediaFormat.createVideoFormat(codecName, width, height);

            Log.i(TAG, "after create m_format");

            // "video/avc"  - H.264/AVC video
            // "video/hevc" - H.265/HEVC video
            m_codec = MediaCodec.createDecoderByType(codecName);
            m_hasCodecFailed = false;

            Log.i(TAG, "after create m_codec");

            m_surfaceTexture = new SurfaceTexture(0, false);
            Log.i(TAG, "after create m_surface");

            m_surface = new Surface(m_surfaceTexture);
            m_codec.configure(m_format, m_surface, null, 0);

            m_codec.start();
            m_inputBuffers = m_codec.getInputBuffers();
            Log.i(TAG, "m_codec started");
            return true;
        }
        catch (MediaCodec.CodecException e)
        {
            Log.i(TAG, "CodecException: " + e.toString());
            Log.i(TAG, "CodecException message: " + e.getMessage());
            releaseDecoder();
            return false;
        }
        catch (java.io.IOException e)
        {
            Log.i(TAG, "m_codec start failed");
            releaseDecoder();
            return false;
        }
        catch (Exception e)
        {
            Log.i(TAG, "Exception: " + e.toString());
            Log.i(TAG, "Exception message: " + e.getMessage());
            releaseDecoder();
            return false;
        }
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

    public int maxDecoderWidth(String mimeType)
    {
        // Requires API level 21.
        int currentapiVersion = android.os.Build.VERSION.SDK_INT;
        if (currentapiVersion < android.os.Build.VERSION_CODES.LOLLIPOP)
            return 1920;

        try
        {
            MediaCodecInfo codecInfo = selectCodec(mimeType);
            CodecCapabilities caps = codecInfo.getCapabilitiesForType(mimeType);
            VideoCapabilities vcaps = caps.getVideoCapabilities();
            return vcaps.getSupportedWidths().getUpper();
        }
        catch (Exception e)
        {
            return -1;
        }
    }

    public int maxDecoderHeight(String mimeType)
    {
        // Requires API level 21.
        int currentapiVersion = android.os.Build.VERSION.SDK_INT;
        if (currentapiVersion < android.os.Build.VERSION_CODES.LOLLIPOP)
            return 1080;

        try
        {
            MediaCodecInfo codecInfo = selectCodec(mimeType);
            CodecCapabilities caps = codecInfo.getCapabilitiesForType(mimeType);
            VideoCapabilities vcaps = caps.getVideoCapabilities();
            return vcaps.getSupportedHeights().getUpper();
        }
        catch (Exception e)
        {
            return -1;
        }
    }

    /**
     * @return Either frame number of the decoded frame, or 0 if no frames left, or kCodecFailed,
     * or kNoInputBuffers, or a different negative value on other errors.
     */
    public long decodeFrame(long srcDataPtr, int frameSize, long frameNum)
    {
        if (m_hasCodecFailed)
        {
            //Log.i(TAG, "decodeFrame() called on failed cocec.");
            return kCodecFailed;
        }

        try
        {
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
                Log.i(TAG, "error dequeueInputBuffer");
                return kNoInputBuffers; //< No input buffers left.
            }

            for (;;)
            {
                MediaCodec.BufferInfo info = new MediaCodec.BufferInfo();
                int outputBufferId =
                    m_codec.dequeueOutputBuffer(info, m_gotOutputData ? timeoutUs : 1000 * 10);
                //Log.i(TAG, "dequee buffer result=" + outputBufferId);
                switch (outputBufferId)
                {
                    case MediaCodec.INFO_OUTPUT_BUFFERS_CHANGED:
                        //Log.i(TAG, "MediaCodec.INFO_OUTPUT_BUFFERS_CHANGED");
                        break;
                    case MediaCodec.INFO_OUTPUT_FORMAT_CHANGED:
                        m_format = m_codec.getOutputFormat(); // option B
                        //Log.i(TAG, "MediaCodec.INFO_OUTPUT_FORMAT_CHANGED");
                        m_gotOutputData = true;
                        break;
                    case MediaCodec.INFO_TRY_AGAIN_LATER:
                        //Log.i(TAG, "MediaCodec.INFO_TRY_AGAIN_LATER");
                        return 0; // no error
                    default:
                        long outFrameNum = info.presentationTimeUs;
                        m_codec.releaseOutputBuffer(outputBufferId, /*render*/ true);
                        return outFrameNum;
                }
            }
        }
        catch (MediaCodec.CodecException e)
        {
            m_hasCodecFailed = true;

            Log.i(TAG, "CodecException: " + e.toString());
            Log.i(TAG, "CodecException message: " + e.getMessage());

            if (e.isTransient())
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
        catch (IllegalStateException e)
        {
            Log.i(TAG, "IllegalStateException: " + e.toString());
            Log.i(TAG, "IllegalStateException message: " + e.getMessage());
            return -1;
        }
        catch (Exception e)
        {
            Log.i(TAG, "Exception: " + e.toString());
            Log.i(TAG, "Exception message: " + e.getMessage());
            return -1;
        }
    }

    /**
     * @return Either frame number of the flushed frame, or 0 if no frames left, or kCodecFailed.
     */
    public long flushFrame(long timeoutUsec)
    {
        if (m_hasCodecFailed)
        {
            //Log.i(TAG, "flushFrame() called on failed cocec.");
            return kCodecFailed;
        }

        try
        {
            //m_codec.flush();
            //Log.i(TAG, "flushFrame(" + timeoutUsec + ") BEGIN");
            MediaCodec.BufferInfo info = new MediaCodec.BufferInfo();
            int outputBufferId = m_codec.dequeueOutputBuffer(info, timeoutUsec);
            switch (outputBufferId)
            {
                case MediaCodec.INFO_OUTPUT_BUFFERS_CHANGED:
                case MediaCodec.INFO_OUTPUT_FORMAT_CHANGED:
                case MediaCodec.INFO_TRY_AGAIN_LATER:
                    //Log.i(TAG, "flushFrame(" + timeoutUsec + ") END -> 0");
                    return 0; // no more frames left
                default:
                    long outFrameNum = info.presentationTimeUs;
                    m_codec.releaseOutputBuffer(outputBufferId, /*render*/ true);
                    //Log.i(TAG, "flushFrame(" + timeoutUsec + ") END -> " + outFrameNum);
                    return outFrameNum;
            }
        }
        catch (MediaCodec.CodecException e)
        {
            m_hasCodecFailed = true;

            Log.i(TAG, "CodecException: " + e.toString());
            Log.i(TAG, "CodecException message: " + e.getMessage());

            if (e.isTransient())
                return -1;
            else
                return kCodecFailed;
        }
        catch (IllegalStateException e)
        {
            Log.i(TAG, "IllegalStateException" + e.toString());
            Log.i(TAG, "IllegalStateException" + e.getMessage());
            Log.i(TAG, "flushFrame(" + timeoutUsec + ") END -> -1");
            return -1;
        }
        catch (Exception e)
        {
            Log.i(TAG, "Exception" + e.toString());
            Log.i(TAG, "Exception" + e.getMessage());
            Log.i(TAG, "flushFrame(" + timeoutUsec + ") END -> -1");
            return -1;
        }
    }

    public void updateTexImage()
    {
        m_surfaceTexture.updateTexImage();
    }

    public void getTransformMatrix (float[] mtx)
    {
        m_surfaceTexture.getTransformMatrix(mtx);
    }

    public void releaseDecoder()
    {
        //Log.i(TAG, "releaseDecoder() BEGIN");

        if (m_codec != null)
        {
            if (!m_hasCodecFailed)
                m_codec.stop();
            //Log.i(TAG, "releaseDecoder(): stop() finished, calling release()...");
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

        //Log.i(TAG, "releaseDecoder() END");
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
