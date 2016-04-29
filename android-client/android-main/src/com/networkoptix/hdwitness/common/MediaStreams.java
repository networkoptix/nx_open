package com.networkoptix.hdwitness.common;

import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

import android.util.Log;

final public class MediaStreams {

    public static enum MediaTransport {
        HTTP, RTSP;

        public static MediaTransport getDefault() {
            if (Utils.hasIcecream())
                return HTTP;    
            return RTSP;
        }

        public static MediaTransport getDefault(Set<MediaTransport> allowed) {
            if (Utils.hasIcecream() && allowed.contains(HTTP))
                return HTTP;            
            if (allowed.contains(RTSP))
                return RTSP;
            return RTSP;
        }
    }

    private static enum Codec {
        CODEC_ID_H263P, CODEC_ID_H263I, CODEC_ID_H264;
    }

    @SuppressWarnings("serial")
    private HashMap<Codec, Integer> mCodecsIds = new HashMap<Codec, Integer>() {
        {
            put(Codec.CODEC_ID_H263P, 20);
            put(Codec.CODEC_ID_H263I, 21);
            put(Codec.CODEC_ID_H264, 28);
        }
    };

    @SuppressWarnings("serial")
    private Set<Integer> mSupportedCodecs = new HashSet<Integer>() {
        {
            addAll(mCodecsIds.values());
        }
    };

    final public class MediaStream {
        int codec;
        int encoderIndex;
        String resolution;
        boolean transcodingRequired;
        List<String> transports;

        @Override
        public String toString() {
            StringBuilder sb = new StringBuilder();
            sb.append("{\n");
            sb.append("codec:");
            sb.append(codec);
            sb.append(",\n");
            sb.append("encoderIndex:");
            sb.append(encoderIndex);
            sb.append(",\n");
            sb.append("resolution:");
            sb.append(resolution);
            sb.append(",\n");
            sb.append("transcodingRequired:");
            sb.append(transcodingRequired);
            sb.append(",\n");
            sb.append("transports:");
            sb.append(transports);
            sb.append("\n");
            sb.append("}");
            return sb.toString();
        }
    }

    final public class JsonMediaStreamsWrapper {
        List<MediaStream> streams;
    }

    private ArrayList<MediaStream> mStreams = new ArrayList<MediaStream>();
    private HashMap<MediaTransport, ArrayList<Resolution>> mAllowedResolutions = new HashMap<MediaTransport, ArrayList<Resolution>>();
    private HashSet<MediaTransport> mAllowedTransports = new HashSet<MediaTransport>();

    public MediaStreams(JsonMediaStreamsWrapper wrapper) {
        mStreams.addAll(wrapper.streams);

        Set<Resolution> rtspResolutionsSet = new HashSet<Resolution>();
        Set<Resolution> httpResolutionsSet = new HashSet<Resolution>();

        for (final MediaStream stream : mStreams) {

            boolean isNative = !stream.transcodingRequired;

            /* Ignore unsupported codecs. */
            if (isNative && !mSupportedCodecs.contains(stream.codec))
                continue;

            if (stream.resolution.equals("*") && !isNative) {
                if (stream.transports.contains("webm"))
                    httpResolutionsSet.addAll(Resolution.STANDARD_RESOLUTIONS);
                if (stream.transports.contains("rtsp"))
                    rtspResolutionsSet.addAll(Resolution.STANDARD_RESOLUTIONS);
                continue;
            }

            Resolution res = new Resolution(stream.resolution);
            res.setNative(isNative);
            res.setStreamIndex(stream.encoderIndex);

            if (stream.transports.contains("webm"))
                httpResolutionsSet.add(res);

            if (stream.transports.contains("rtsp"))
                rtspResolutionsSet.add(res);
        }

        ArrayList<Resolution> rtspResolutions = new ArrayList<Resolution>(rtspResolutionsSet);
        ArrayList<Resolution> httpResolutions = new ArrayList<Resolution>(httpResolutionsSet);
        Collections.sort(rtspResolutions);
        Collections.sort(httpResolutions);

        mAllowedResolutions.put(MediaTransport.RTSP, rtspResolutions);
        mAllowedResolutions.put(MediaTransport.HTTP, httpResolutions);
        if (!rtspResolutions.isEmpty())
            mAllowedTransports.add(MediaTransport.RTSP);
        if (!httpResolutions.isEmpty())
            mAllowedTransports.add(MediaTransport.HTTP);

        Log.d("MediaStreams", "Streams loaded:");
        Log.d("MediaStreams", toString());
        Log.d("MediaStreams", "Parsed to:");
        Log.d("MediaStreams", rtspResolutions.toString());
        Log.d("MediaStreams", httpResolutions.toString());
    }

    @Override
    public String toString() {
        return mStreams.toString();
    }

    public ArrayList<Resolution> allowedResolutions(MediaTransport transport) {
        return mAllowedResolutions.get(transport);

    }

    public HashSet<MediaTransport> allowedTransports() {
        return mAllowedTransports;
    }

    public double aspectRatio() {
        for (final MediaStream stream: mStreams) {
            if (stream.transcodingRequired || stream.encoderIndex != 0)
                continue;
            Resolution res = new Resolution(stream.resolution);
            if (!res.isValid() || res.getWidth() <= 0 || res.getHeight() <= 0)
                continue;
            
            return ((double)res.getWidth()) / res.getHeight();
        }
        
        /* Defaulting to max standard aspect ratio. */
        return 16.0 / 9.0;
    }

}
