## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

# We cannot rely on the detected value as it breaks dependency lookup as soon as a developer
# installs an Xcode update. Conan should look for packages built with Xcode version on the CI
# builders, not local version.
[settings]
compiler.version=15
