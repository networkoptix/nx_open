/*
 * Copyright (c) 2016, NVIDIA CORPORATION. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *  * Neither the name of NVIDIA CORPORATION nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _ARGUS_EVENT_H
#define _ARGUS_EVENT_H

namespace Argus
{

/**
 * An Event represents an asynchronous result.
 *
 * Every Event will have a single EventType and will expose one or more
 * interfaces, with the core IEvent interface being mandatory.
 */
class Event : public InterfaceProvider
{
protected:
    ~Event() {}
};

/**
 * A unique identifier for a particular type of Event.
 */
class EventType : public NamedUUID
{
public:
    EventType(uint32_t time_low_
            , uint16_t time_mid_
            , uint16_t time_hi_and_version_
            , uint16_t clock_seq_
            , uint8_t c0, uint8_t c1, uint8_t c2, uint8_t c3, uint8_t c4, uint8_t c5
            , const char* name)
    : NamedUUID(time_low_, time_mid_, time_hi_and_version_, clock_seq_,
                c0, c1, c2, c3, c4, c5, name)
    {}

    EventType()
    : NamedUUID(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, "EVENT_TYPE_UNSPECIFIED")
    {}
};


/*
 * Core Event types
 */

/// Event Type used to report an error.
/// Additional interface: IEventError.
DEFINE_UUID(EventType, EVENT_TYPE_ERROR,            2c80d8b0,2bfd,11e5,a2cb,08,00,20,0c,9a,66);

/// Event Type used to report when a capture starts.
DEFINE_UUID(EventType, EVENT_TYPE_CAPTURE_STARTED,  2c80d8b1,2bfd,11e5,a2cb,08,00,20,0c,9a,66);

/// Event Type used to report when all capture processing has completed.
DEFINE_UUID(EventType, EVENT_TYPE_CAPTURE_COMPLETE, 2c80d8b2,2bfd,11e5,a2cb,08,00,20,0c,9a,66);


/**
 * @class IEvent
 *
 * The interface common to all event types.
 */
DEFINE_UUID(InterfaceID, IID_EVENT, 98bcb49e,fd7d,11e4,a322,16,97,f9,25,ec,7b);

class IEvent : public Interface
{
public:
    static const InterfaceID& id() { return IID_EVENT; }

    /**
     * Returns the event type.
     */
    virtual EventType getEventType() const = 0;

    /**
     * Returns the time of the event, in nanoseconds.
     */
    virtual uint64_t getTime() const = 0;

    /**
     * Returns the capture id for the event.
     */
    virtual uint32_t getCaptureId() const = 0;

protected:
    ~IEvent() {}
};

/**
 * @class IEventError
 *
 * The interface that exposes the properties for an ERROR event.
 */
DEFINE_UUID(InterfaceID, IID_EVENT_ERROR, 13e0fc70,1ab6,11e5,b939,08,00,20,0c,9a,66);

class IEventError : public Interface
{
public:
    static const InterfaceID& id() { return IID_EVENT_ERROR; }

    /**
     * Returns the Status value describing the error.
     */
    virtual Status getStatus() const = 0;

protected:
    ~IEventError() {}
};

/**
 * @class IEventCaptureComplete
 *
 * The interface that exposes the properties for a CAPTURE_COMPLETE event.
 */
DEFINE_UUID(InterfaceID, IID_EVENT_CAPTURE_COMPLETE, 8b2b40b5,f1e4,4c4d,ae1c,f3,93,f6,54,06,d5);

class IEventCaptureComplete : public Interface
{
public:
    static const InterfaceID& id() { return IID_EVENT_CAPTURE_COMPLETE; }

    /**
     * Returns all dynamic metadata associated with this capture.
     * The lifetime of the returned pointer is equivalent to the lifetime of this event.
     * NULL may be returned if no metadata is available because the
     * capture failed or was aborted.
     */
    virtual const CaptureMetadata* getMetadata() const = 0;

    /**
     * Returns the error status of the metadata event.
     * If this value is not STATUS_OK, getMetadata() will return NULL.
     */
    virtual Status getStatus() const = 0;

protected:
    ~IEventCaptureComplete() {}
};

} // namespace Argus

#endif // _ARGUS_EVENT_H
