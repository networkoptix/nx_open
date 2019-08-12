#pragma once

namespace nx::cloud::storage::service::api {

static constexpr char kStoragePrefix[] = "/storage";
static constexpr char kStorages[] = "/storages/";
static constexpr char kStorageId[] = "/storage/{id}";

static constexpr char kCameras[] = "/storage/{id}/cameras";

static constexpr char kAwsBuckets[] = "/aws_buckets/";
static constexpr char kAwsBucketName[] = "/aws_buckets/{name}";

static constexpr char kStorageCredentials[] = "/storage/{id}/ioDevice/awss3/credentials";

} // namespace nx::cloud::storage::service::api
