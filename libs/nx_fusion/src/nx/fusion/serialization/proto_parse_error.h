// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#ifndef QN_PROTO_PARSE_ERROR_H
#define QN_PROTO_PARSE_ERROR_H

struct QnProtoParseError {
    enum ParseError {
        NoError = 0,
        InvalidMarker = 1,
        UnexpectedTermination = 2,
        VariantTooLong = 3
    };

    QnProtoParseError(): offset(0), error(NoError) {}
    QnProtoParseError(int offset, ParseError error): offset(offset), error(error) {}

    int offset;
    ParseError error;
};

#endif // QN_PROTO_PARSE_ERROR_H
