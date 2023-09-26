// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <cstdint>
#include <QString>

#include <nx/utils/basic_factory.h>

namespace nx::vms::metadata {

class AbstractMetadataBinaryFile
{
public:
    virtual ~AbstractMetadataBinaryFile() = default;

    virtual void close() = 0;
    virtual void setFileName(const QString& fName) = 0;
    virtual bool openReadOnly() = 0;
    virtual bool openRW() = 0;
    virtual bool resize(int64_t sz) = 0;
    virtual bool seek(int64_t offset) = 0;
    virtual int64_t read(char* data, int64_t maxSize) = 0;
    virtual QString fileName() const = 0;
    virtual bool isOpen() const = 0;
    virtual int64_t size() const = 0;
    virtual int64_t write(const char* buffer, qint64 count) = 0;
    virtual bool flush() = 0;
};

using MetadataBinaryFileFactoryFunc = std::unique_ptr<AbstractMetadataBinaryFile>();

class NX_VMS_COMMON_API MetadataBinaryFileFactory:
    public nx::utils::BasicFactory<MetadataBinaryFileFactoryFunc>
{
    using base_type = nx::utils::BasicFactory<MetadataBinaryFileFactoryFunc>;

public:
    MetadataBinaryFileFactory();

    static MetadataBinaryFileFactory& instance();
};

} // namespace nx::vms::metadata
