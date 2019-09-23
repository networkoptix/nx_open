#pragma once

namespace nx::cloud::storage::service::api {

static constexpr char kStoragePrefix[] = "/storage";
static constexpr char kStorages[] = "/storages/";
static constexpr char kStorage[] = "/storage/{storageId}";
static constexpr char kSystems[] = "/storage/{storageId}/systems/";
static constexpr char kSystem[] = "/storage/{storageId}/system/{systemId}";

static constexpr char kCameras[] = "/storage/{storageId}/cameras";

static constexpr char kAwsBucketsPrefix[] = "/aws_buckets";
static constexpr char kAwsBuckets[] = "/aws_buckets/";
static constexpr char kAwsBucket[] = "/aws_buckets/{bucketName}";

static constexpr char kStorageCredentials[] = "/storage/{storageId}/ioDevice/awss3/credentials";

} // namespace nx::cloud::storage::service::api
