package com.networkoptix.hdwitness.api.ec2;

import java.io.IOException;
import java.io.InputStream;
import java.nio.ByteBuffer;

import com.networkoptix.hdwitness.api.ec2.TransactionLogParser.HttpStreamReader;

public class ChunkedHttpStreamReader implements HttpStreamReader {

    private static final int MAXIMUM_PACKET_SIZE = 256 * 1024 * 1024; // 256 Mb is far too large packet

    private final InputStream mStream;
    
    public ChunkedHttpStreamReader(InputStream stream) {
        mStream = stream;
    }
   

    @Override
    public String readTransaction() throws IOException {
        byte[] packetSize = new byte[4];

        mStream.read(packetSize);

        final ByteBuffer buf = ByteBuffer.wrap(packetSize);
        final int size = buf.getInt();
        
        if (size > MAXIMUM_PACKET_SIZE)
            throw new IOException("Invalid Packet size");

        byte[] data = new byte[size];
        int bytesRead = 0;

        do {
            final int bytes = mStream.read(data, bytesRead, size - bytesRead);
            bytesRead += bytes;
        } while (bytesRead < size);

        return new String(data);
    }

    
    
}
