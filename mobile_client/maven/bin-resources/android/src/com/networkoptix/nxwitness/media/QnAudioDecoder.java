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

public class QnMediaDecoder {


    public boolean init(String codecName)
    {
        try
        {
            System.out.println("codecName=" + codecName);

            format = MediaFormat.createAudioFormat(codecName);
            codec = MediaCodec.createDecoderByType(codecName);
            codec.configure(format, null, null, 0);

            codec.start();
            inputBuffers = codec.getInputBuffers();
            System.out.println("codec started");
            return true;
        } catch(java.io.IOException e)
        {
            System.out.println("codec start failed");
            return false;
        }
        catch(Exception e)
        {
            System.out.println("Exception" + e.toString());
            System.out.println("Exception" + e.getMessage());
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

    public boolean decodeFrame(long srcDataPtr, int frameSize, long cObject)
    {
        try
        {
            final long timeoutUs = 1000 * 3000; //< 3 second
            int inputBufferId = codec.dequeueInputBuffer(timeoutUs);
            if (inputBufferId >= 0)
            {
              ByteBuffer inputBuffer = inputBuffers[inputBufferId];
              inputBuffer.allocateDirect(frameSize);
              fillInputBuffer(inputBuffer, srcDataPtr, frameSize); //< C++ callback
              codec.queueInputBuffer(inputBufferId, 0, frameSize, 0, 0);
            }
            else {
                System.out.println("error dequeueInputBuffer");
                return false;
            }

            MediaCodec.BufferInfo info = new MediaCodec.BufferInfo();
            int outputBufferId = codec.dequeueOutputBuffer(info, 0);

            if (outputBufferId >= 0)
            {
                ByteBuffer outputBuffer = codec.getOutputBuffer(outputBufferId);
                readOutputBuffer(cObject, outputBuffer, outputBuffer.capacity());
                codec.releaseOutputBuffer(outputBufferId, false);
                return true;
            }

            return false;
              
        }
        catch(IllegalStateException e)
        {
            System.out.println("IllegalStateException" + e.toString());
            System.out.println("IllegalStateException" + e.getMessage());
            return false;
        }
        catch(Exception e)
        {
            System.out.println("Exception" + e.toString());
            System.out.println("Exception" + e.getMessage());
            return false;
        }
    }

    private static native void fillInputBuffer(ByteBuffer buffer,  long srcDataPtr, int frameSize);
    private static native void readOutputBuffer(long cObject, ByteBuffer buffer, int bufferSize);

    ByteBuffer[] inputBuffers;
    private MediaCodec codec;
    private MediaFormat format;
}
