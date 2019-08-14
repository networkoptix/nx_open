#pragma once

namespace nx::cloud::storage::service::api {

static constexpr char kStoragePrefix[] = "/storage";
static constexpr char kStorages[] = "/storages/";
static constexpr char kStorageId[] = "/storage/{storageId}";
static constexpr char kStorageIdSystems[] = "/storage/{storageId}/systems/";
static constexpr char kStorageIdSystemId[] = "/storage/{storageId}/system/{systemId}";

static constexpr char kCameras[] = "/storage/{storageId}/cameras";

static constexpr char kAwsBuckets[] = "/aws_buckets/";
static constexpr char kAwsBucketName[] = "/aws_buckets/{name}";

static constexpr char kStorageCredentials[] = "/storage/{storageId}/ioDevice/awss3/credentials";

} // namespace nx::cloud::storage::service::api
