#pragma once

#include <nx/network/rest/handler.h>
#include <utils/common/request_param.h>

using RestPermissions = nx::network::rest::Permissions;

// TODO: This class should be removed as soon as old style handlers are refactored.
class QnRestRequestHandler: public nx::network::rest::Handler
{
protected:
    virtual nx::network::rest::Response executeGet(const nx::network::rest::Request& request);
    virtual nx::network::rest::Response executeDelete(const nx::network::rest::Request& request);
    virtual nx::network::rest::Response executePost(const nx::network::rest::Request& request);
    virtual nx::network::rest::Response executePut(const nx::network::rest::Request& request);

    virtual int executeGet(
        const QString& /*path*/,
        const QnRequestParamList& /*params*/,
        QByteArray& /*result*/,
        QByteArray& /*contentType*/,
        const QnRestConnectionProcessor* /*owner*/);

    virtual int executeDelete(
        const QString& /*path*/,
        const QnRequestParamList& /*params*/,
        QByteArray& /*result*/,
        QByteArray& /*contentType*/,
        const QnRestConnectionProcessor* /*owner*/);

    virtual int executePost(
        const QString& /*path*/,
        const QnRequestParamList& /*params*/,
        const QByteArray& /*body*/,
        const QByteArray& /*srcBodyContentType*/,
        QByteArray& /*result*/,
        QByteArray& /*resultContentType*/,
        const QnRestConnectionProcessor* /*owner*/);

    virtual int executePut(
        const QString& /*path*/,
        const QnRequestParamList& /*params*/,
        const QByteArray& /*body*/,
        const QByteArray& /*srcBodyContentType*/,
        QByteArray& /*result*/,
        QByteArray& /*resultContentType*/,
        const QnRestConnectionProcessor* /*owner*/);

    virtual void afterExecute(
        const QString& /*path*/,
        const QnRequestParamList& /*params*/,
        const QByteArray& /*body*/,
        const QnRestConnectionProcessor* /*owner*/);
};
