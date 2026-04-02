[settings]
os=Windows
arch=x86_64
build_type=Release
compiler=msvc
compiler.version=193
compiler.cppstd=17
compiler.runtime=dynamic

[conf]
mongo-c-driver/*:tools.cmake.cmaketoolchain:generator=Ninja
