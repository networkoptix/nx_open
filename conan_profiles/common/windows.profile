include(default)
include(common.profile)

[settings]
os=Windows
compiler=Visual Studio
compiler.version=16
compiler.cppstd=20
qt/*:compiler.version=17

[options]
libpq/*:shared=True
opencv-static/*:cuda_arch_bin=5.0
opencv-static/*:cudawarping=True
opencv-static/*:with_cuda=True
qt/*:psql=True

[tool_requires]
opencv-static/*:cuda-toolkit/12.5.1
opencv-static/*:cuda-toolkit/11.8
openssl/*:strawberryperl/5.32.1.1,nasm/2.16.01
qt*:patch-windows/0.1,strawberryperl/5.32.1.1
