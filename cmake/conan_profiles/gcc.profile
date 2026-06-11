## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

# When a Conan profile declares a build dependency without a revision and the recipe declares the
# same dependency with revision, the revision is ignored by Conan. However another profile can
# request a certain revision for a dependency. This is a profile to work-around the issue for
# gcc-toolchain dependency.
[tool_requires]
gcc-toolchain/13.3#54cc3c576f5d9f324caaf23152b4074e

# Pin clang recipe revision here until we update to conan 2.30+
clang/20.1.2#493f55bfbb20874208a25ee845a83c3c
