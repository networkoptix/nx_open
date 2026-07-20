## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

# When a Conan profile declares a build dependency without a revision and the recipe declares the
# same dependency with revision, the revision is ignored by Conan. However another profile can
# request a certain revision for a dependency. This is a profile to work-around the issue for
# gcc-toolchain dependency.
[tool_requires]
gcc-toolchain/13.3#54cc3c576f5d9f324caaf23152b4074e
