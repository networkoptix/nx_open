include(default)
include(common.profile)

[settings]
compiler=clang
compiler.version=17
compiler.libcxx=c++_shared
compiler.cppstd=20

os=Android
os.api_level=22

[tool_requires]
openal/*:AndroidNDK/r26d
