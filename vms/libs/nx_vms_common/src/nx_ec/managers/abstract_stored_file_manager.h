// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/utils/async_handler_executor.h>

#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtCore/QByteArray>

#include "../ec_api_common.h"

namespace ec2 {

class NX_VMS_COMMON_API AbstractStoredFileNotificationManager: public QObject
{
    Q_OBJECT

signals:
    void added(QString filename);
    void updated(QString filename);
    void removed(QString filename);
};

/*!
    \note All methods are asynchronous if other not specified
*/
class NX_VMS_COMMON_API AbstractStoredFileManager
{
public:
    virtual ~AbstractStoredFileManager() = default;

    virtual int getStoredFile(
        const QString& fileName,
        Handler<QByteArray> handler,
        nx::utils::AsyncHandlerExecutor handlerExecutor = {}) = 0;

    ErrorCode getStoredFileSync(const QString& fileName, QByteArray* outFileData);

    virtual int addStoredFile(
        const QString& fileName,
        const QByteArray& fileData,
        Handler<> handler,
        nx::utils::AsyncHandlerExecutor handlerExecutor = {}) = 0;

    ErrorCode addStoredFileSync(const QString& fileName, const QByteArray& fileData);

    virtual int deleteStoredFile(
        const QString& fileName,
        Handler<> handler,
        nx::utils::AsyncHandlerExecutor handlerExecutor = {}) = 0;

    ErrorCode deleteStoredFileSync(const QString& fileName);

    virtual int listDirectory(
        const QString& directoryName,
        Handler<QStringList> handler,
        nx::utils::AsyncHandlerExecutor handlerExecutor = {}) = 0;

    ErrorCode listDirectorySync(const QString& directoryName, QStringList* outNameList);
};

} // namespace ec2
