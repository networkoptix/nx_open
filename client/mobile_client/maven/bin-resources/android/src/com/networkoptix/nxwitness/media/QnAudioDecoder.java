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
import java.nio.ByteBuffer;
import org.qtproject.qt5.android.QtNative;
import android.media.MediaCodecInfo;
import android.media.MediaCodecInfo.CodecCapabilities;
import android.media.MediaCodecInfo.VideoCapabilities;
import android.media.MediaCodecList;
import android.os.Build;

public class QnAudioDecoder 
{

    static public boolean isDecoderCompatibleToPlatform()
    {
        if (android.os.Build.VERSION.SDK_INT < android.os.Build.VERSION_CODES.LOLLIPOP)
            return true;
        final boolean is64bit = Build.SUPPORTED_64_BIT_ABIS.length > 0;
        return !is64bit;
    }


    public boolean init(String codecName,  int sampleRate, int channelCount,
                        long extraDataPtr, int extraDataSize)
    {
        try
        {
            System.out.println("Audio codecName=" + codecName + "sampleRate=" + sampleRate + "channels" + channelCount);

            format = MediaFormat.createAudioFormat(codecName, sampleRate, channelCount);
            if (extraDataSize > 0)
            {
                System.out.println("Set Audio extraData. size=" + extraDataSize);
                ByteBuffer extraDataBuffer = ByteBuffer.allocateDirect(extraDataSize);
                fillInputBuffer(extraDataBuffer, extraDataPtr, extraDataSize, extraDataBuffer.capacity()); //< C++ callback
                format.setByteBuffer("csd-0", extraDataBuffer);
            }

            codec = MediaCodec.createDecoderByType(codecName);
            codec.configure(format, null, null, 0);

            codec.start();
            System.out.println("audio codec started");
            inputBuffers = codec.getInputBuffers();
            outputBuffers = codec.getOutputBuffers();
            return true;
        } catch(java.io.IOException e)
        {
            System.out.println("audio codec start failed");
            return false;
        }
        catch(Exception e)
        {
            System.out.println("Audio start failed. Exception" + e.toString());
            System.out.println("Audio start failed. Exception" + e.getMessage());
            return false;
        }
    }

    public void releaseDecoder()
    {
        if (codec != null)
        {
            codec.stop();
            codec.release();
            System.out.println("Decoder release");
        }
    }

    public long decodeFrame(long srcDataPtr, int frameSize, long timestampUs, long cObject)
    {
        try
        {
            final long timeoutUs = 1000 * 3000; //< 3 second
            int inputBufferId = codec.dequeueInputBuffer(timeoutUs);
            if (inputBufferId >= 0)
            {
              ByteBuffer inputBuffer = inputBuffers[inputBufferId];
              fillInputBuffer(inputBuffer, srcDataPtr, frameSize, inputBuffer.capacity()); //< C++ callback
              codec.queueInputBuffer(inputBufferId, 0, frameSize, timestampUs, 0);
            }
            else {
                System.out.println("error dequeueInputBuffer");
                return -1;
            }

            MediaCodec.BufferInfo info = new MediaCodec.BufferInfo();
            int outputBufferId = codec.dequeueOutputBuffer(info, 0);

            switch (outputBufferId)
            {
            case MediaCodec.INFO_OUTPUT_BUFFERS_CHANGED:
                System.out.println("Audio MediaCodec.INFO_OUTPUT_BUFFERS_CHANGED");
                return 0; // no error
            case MediaCodec.INFO_OUTPUT_FORMAT_CHANGED:
                format = codec.getOutputFormat(); // option B
                System.out.println("Audio MediaCodec.INFO_OUTPUT_FORMAT_CHANGED");
                return 0; // no error
            case MediaCodec.INFO_TRY_AGAIN_LATER:
                System.out.println("Audio MediaCodec.INFO_TRY_AGAIN_LATER");
                return 0; // no error
            default:
                long outTimestampUs = info.presentationTimeUs;
                ByteBuffer outputBuffer = outputBuffers[outputBufferId];
                readOutputBuffer(cObject, outputBuffer, info.size);
                codec.releaseOutputBuffer(outputBufferId, false);
                return outTimestampUs;
            }
        }
        catch(IllegalStateException e)
        {
            System.out.println("IllegalStateException" + e.toString());
            System.out.println("IllegalStateException" + e.getMessage());
            return -1;
        }
        catch(Exception e)
        {
            System.out.println("Exception" + e.toString());
            System.out.println("Exception" + e.getMessage());
            return -1;
        }
    }

    private static native void fillInputBuffer(ByteBuffer buffer,  long srcDataPtr, int frameSize, int capacity);
    private static native void readOutputBuffer(long cObject, ByteBuffer buffer, int bufferSize);

    ByteBuffer[] inputBuffers;
    ByteBuffer[] outputBuffers;
    private MediaCodec codec;
    private MediaFormat format;
}
