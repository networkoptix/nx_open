include(default)
include(common.profile)

[settings]
os=Windows
compiler=msvc
compiler.version=193
compiler.cppstd=20
cpython*:build_type=Release
cpython*:compiler.runtime=dynamic
compiler.runtime=dynamic

[options]
cpython*:shared = True
libpq/*:shared=True
opencv-static/*:cuda_arch_bin=7.5
opencv-static/*:cudawarping=True
opencv-static/*:with_cuda=True
qt/*:psql=True

[tool_requires]
opencv-static/*:cuda-toolkit/12.8.1
openssl/*:strawberryperl/5.30.0.1,nasm/2.16.01
qt*:patch-windows/0.1,strawberryperl/5.30.0.1,clang/20.1.2,ninja/1.12.1
