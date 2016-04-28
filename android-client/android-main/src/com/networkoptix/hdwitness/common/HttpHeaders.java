package com.networkoptix.hdwitness.common;

import java.util.HashMap;
import java.util.Iterator;
import java.util.Map;
import java.util.Map.Entry;

public class HttpHeaders implements Iterable<HttpHeaders.HttpHeader> {

    public class HttpHeader {
        public final String key;
        public final String value;
        
        HttpHeader(String _key, String _value) {
            key = _key;
            value = _value;
        }
    }
    
    private Map<String, String> mHeaders = new HashMap<String, String>();
    
    public HttpHeaders() {
    }

    public void clear() {
        mHeaders.clear();
    }

    public boolean isEmpty() {
        return mHeaders.isEmpty();
    }

    public void add(String key, String value) {
        mHeaders.put(key, value);
    }

    @Override
    public Iterator<HttpHeader> iterator() {
        return new Iterator<HttpHeaders.HttpHeader>() {

            private final Iterator<Entry<String, String>> iter = mHeaders.entrySet().iterator();
            
            @Override
            public boolean hasNext() {
                return iter.hasNext();
            }

            @Override
            public HttpHeader next() {
                Entry<String, String> entry = iter.next();
                return new HttpHeader(entry.getKey(), entry.getValue());
            }

            @Override
            public void remove() {
                throw new UnsupportedOperationException("no changes allowed");
            }
        };
    }

    public Map<String, String> toMap() {
        return mHeaders;
    }
 
    
    
    
}
