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

public class QnMediaDecoder {


    public boolean init(String codecName, int width, int height)
    {
        try
        {
            System.out.println("codecName=" + codecName);
            System.out.print("width=");
            System.out.println(width);
            System.out.print("height=");
            System.out.println(height);

            format = MediaFormat.createVideoFormat(codecName, width, height);

            System.out.println("after create format");

            // "video/avc"  - H.264/AVC video
            // "video/hevc" - H.265/HEVC video
            codec = MediaCodec.createDecoderByType(codecName);

            System.out.println("after create codec");

            surfaceTexture = new SurfaceTexture(0, false);
            System.out.println("after create surface");

            surface = new Surface(surfaceTexture);
            codec.configure(format, surface, null, 0);

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

    public long decodeFrame(long srcDataPtr, int frameSize, long frameNum)
    {
        try
        {
            final long timeoutUs = 1000*10000;
            int inputBufferId = codec.dequeueInputBuffer(timeoutUs);
            if (inputBufferId >= 0)
            {
              //ByteBuffer inputBuffer = codec.getInputBuffer(inputBufferId);
              ByteBuffer inputBuffer = inputBuffers[inputBufferId];
              inputBuffer.allocateDirect(frameSize);
              fillInputBuffer(inputBuffer, srcDataPtr, frameSize); //< C++ callback
              codec.queueInputBuffer(inputBufferId, 0, frameSize, frameNum, 0);
            }
            else {
                System.out.println("error dequeueInputBuffer");
                return -1; // error
            }

            MediaCodec.BufferInfo info = new MediaCodec.BufferInfo();
            int outputBufferId = codec.dequeueOutputBuffer(info, 0);
            if (outputBufferId >= 0)
            {
              long outFrameNum = info.presentationTimeUs;
              codec.releaseOutputBuffer(outputBufferId, true); // true means buffer will be send to render surface
              return outFrameNum;
            }
            else if (outputBufferId == MediaCodec.INFO_OUTPUT_FORMAT_CHANGED)
            {
              // Subsequent data will conform to new format.
              // Can ignore if using getOutputFormat(outputBufferId)
              format = codec.getOutputFormat(); // option B
              return 0;
            }
            else {
                System.out.println("error getting outputBufferId");
                return 0; //< no data available
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

    public void updateTexImage()
    {
        surfaceTexture.updateTexImage();
    }

    public void getTransformMatrix (float[] mtx)
    {
        surfaceTexture.getTransformMatrix(mtx);
    }

    protected void finalize()
    {
        if (codec != null)
        {
            codec.stop();
            codec.release();
        }
    }


    private static native void fillInputBuffer(ByteBuffer buffer, long srcDataPtr, int frameSize);

    ByteBuffer[] inputBuffers;
    private MediaCodec codec;
    private MediaFormat format;
    private Surface surface;
    private SurfaceTexture surfaceTexture;
}
