// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QFile>

#include "abstract_metadata_binary_file.h"

namespace nx::vms::metadata {

class NX_VMS_COMMON_API QFileMetadataBinaryFile: public AbstractMetadataBinaryFile
{
public:
    void close() override;
    void setFileName(const QString& fName) override;
    bool openReadOnly() override;
    bool openRW() override;
    bool resize(int64_t sz) override;
    bool seek(int64_t offset) override;
    int64_t read(char* data, int64_t maxSize) override;
    QString fileName() const override;
    bool isOpen() const override;
    int64_t size() const override;
    int64_t write(const char* buffer, qint64 count) override;
    bool flush() override;

private:
    QFile m_file;
};

} // namespace nx::vms::metadata
