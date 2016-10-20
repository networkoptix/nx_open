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
    private static final String TAG = "QnVideoDecoder";

    public boolean init(String codecName, int width, int height)
    {
        try
        {
            gotOutputData = false;

            Log.i(TAG, "codecName: " + codecName);
            Log.i(TAG, "width: " + width);
            Log.i(TAG, "height: " + height);

            format = MediaFormat.createVideoFormat(codecName, width, height);

            Log.i(TAG, "after create format");

            // "video/avc"  - H.264/AVC video
            // "video/hevc" - H.265/HEVC video
            codec = MediaCodec.createDecoderByType(codecName);

            Log.i(TAG, "after create codec");

            surfaceTexture = new SurfaceTexture(0, false);
            Log.i(TAG, "after create surface");

            surface = new Surface(surfaceTexture);
            codec.configure(format, surface, null, 0);

            codec.start();
            inputBuffers = codec.getInputBuffers();
            Log.i(TAG, "codec started");
            return true;
        }
        catch(java.io.IOException e)
        {
            Log.i(TAG, "codec start failed");
            return false;
        }
        catch(Exception e)
        {
            Log.i(TAG, "Exception: " + e.toString());
            Log.i(TAG, "Exception message: " + e.getMessage());
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
        catch(Exception e)
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
        catch(Exception e)
        {
            return -1;
        }
    }

    public long decodeFrame(long srcDataPtr, int frameSize, long frameNum)
    {
        try
        {
            final int kNoInputBuffers = -7;
            final long timeoutUs = 1000 * 1000; //< 1 second
            int inputBufferId = codec.dequeueInputBuffer(timeoutUs);
            if (inputBufferId >= 0)
            {
                ByteBuffer inputBuffer = inputBuffers[inputBufferId];

                // C++ callback.
                fillInputBuffer(inputBuffer, srcDataPtr, frameSize, inputBuffer.capacity());

                codec.queueInputBuffer(inputBufferId, 0, frameSize, frameNum, 0);
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
                    codec.dequeueOutputBuffer(info, gotOutputData ? timeoutUs : 1000 * 10);
                //Log.i(TAG, "dequee buffer result=" + outputBufferId);
                switch (outputBufferId)
                {
                    case MediaCodec.INFO_OUTPUT_BUFFERS_CHANGED:
                        //Log.i(TAG, "MediaCodec.INFO_OUTPUT_BUFFERS_CHANGED");
                        break;
                    case MediaCodec.INFO_OUTPUT_FORMAT_CHANGED:
                        format = codec.getOutputFormat(); // option B
                        //Log.i(TAG, "MediaCodec.INFO_OUTPUT_FORMAT_CHANGED");
                        gotOutputData = true;
                        break;
                    case MediaCodec.INFO_TRY_AGAIN_LATER:
                        //Log.i(TAG, "MediaCodec.INFO_TRY_AGAIN_LATER");
                        return 0; // no error
                    default:
                        long outFrameNum = info.presentationTimeUs;
                        codec.releaseOutputBuffer(outputBufferId, /*render*/ true);
                        return outFrameNum;
                }
            }
        }
        catch(IllegalStateException e)
        {
            Log.i(TAG, "IllegalStateException" + e.toString());
            Log.i(TAG, "IllegalStateException" + e.getMessage());
            return -1;
        }
        catch(Exception e)
        {
            Log.i(TAG, "Exception" + e.toString());
            Log.i(TAG, "Exception" + e.getMessage());
            return -1;
        }
    }

    public long flushFrame(long timeoutUsec)
    {
        try
        {
            //codec.flush();
            //Log.i(TAG, "flushFrame(" + timeoutUsec + ") BEGIN");
            MediaCodec.BufferInfo info = new MediaCodec.BufferInfo();
            int outputBufferId = codec.dequeueOutputBuffer(info, timeoutUsec);
            switch (outputBufferId)
            {
                case MediaCodec.INFO_OUTPUT_BUFFERS_CHANGED:
                case MediaCodec.INFO_OUTPUT_FORMAT_CHANGED:
                case MediaCodec.INFO_TRY_AGAIN_LATER:
                    //Log.i(TAG, "flushFrame(" + timeoutUsec + ") END -> 0");
                    return 0; // no more frames left
                default:
                    long outFrameNum = info.presentationTimeUs;
                    codec.releaseOutputBuffer(outputBufferId, /*render*/ true);
                    //Log.i(TAG, "flushFrame(" + timeoutUsec + ") END -> " + outFrameNum);
                    return outFrameNum;
            }
        }
        catch(IllegalStateException e)
        {
            Log.i(TAG, "IllegalStateException" + e.toString());
            Log.i(TAG, "IllegalStateException" + e.getMessage());
            Log.i(TAG, "flushFrame(" + timeoutUsec + ") END -> -1");
            return -1;
        }
        catch(Exception e)
        {
            Log.i(TAG, "Exception" + e.toString());
            Log.i(TAG, "Exception" + e.getMessage());
            Log.i(TAG, "flushFrame(" + timeoutUsec + ") END -> -1");
            return -1;
        }
    }

    public void updateTexImage()
    {
        surfaceTexture.updateTexImage();
    }

    public void getTransformMatrix (float[] mtx)
    {
        surfaceTexture.getTransformMatrix(mtx);
    }

    public void releaseDecoder()
    {
        if (codec != null)
        {
            //Log.i(TAG, "releaseDecoder() BEGIN");
            codec.stop();
            //Log.i(TAG, "releaseDecoder(): stop() finished, calling release()...");
            codec.release();
            //Log.i(TAG, "releaseDecoder() END");
        }
    }

    private static native void fillInputBuffer(
        ByteBuffer buffer, long srcDataPtr, int frameSize, int capacity);

    ByteBuffer[] inputBuffers;
    private MediaCodec codec;
    private MediaFormat format;
    private Surface surface;
    private SurfaceTexture surfaceTexture;
    private boolean gotOutputData;
}
