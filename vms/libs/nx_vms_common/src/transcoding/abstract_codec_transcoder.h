// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>

#include <nx/media/media_data_packet.h>

//!Base class for all raw media stream transcoders
/*!
    Transcoder receives input stream coded picture at the input and provides output stream coded picture at output.
    It is safe to change output stream parameters during transcoding
*/
class NX_VMS_COMMON_API AbstractCodecTranscoder
{
public:
    virtual ~AbstractCodecTranscoder() {}

    //!Transcode coded picture of input stream to output format
    /*!
        Transcoder is allowed to return NULL coded picture for non-NULL input and return coded output pictures with some delay from input.
        To empty transcoder's coded picture buffer one should provide NULL as input until receiving NULL at output.
        \param media Coded picture of the input stream. May be NULL
        \param result Coded picture of the output stream. If NULL, only decoding is done
        \return return Return error code or 0 if no error
    */
    virtual int transcodePacket(const QnConstAbstractMediaDataPtr& media, QnAbstractMediaDataPtr* const result) = 0;

};
using AbstractCodecTranscoderPtr = std::unique_ptr<AbstractCodecTranscoder>;
