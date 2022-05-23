## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

# When a Conan profile declares a build dependency without a revision and the recipe declares the
# same dependency with revision, the revision is ignored by Conan. However another profile can
# request a certain revision for a dependency. This is a profile to work-around the issue for
# gcc-toolchain dependency.
[build_requires]
gcc-toolchain/10.2#53c2ddb9615885ac85d38be2ec272d8e
