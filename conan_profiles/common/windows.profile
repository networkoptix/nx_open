include(default)
include(common.profile)

[settings]
os=Windows
compiler=Visual Studio
compiler.version=16
compiler.cppstd=20

[options]
libpq/*:shared=True
libvpx/*:shared=False
opencv-static/*:cuda_arch_bin=5.0
opencv-static/*:cudawarping=True
opencv-static/*:with_cuda=True
qt/*:psql=True

[tool_requires]
opencv-static/*:cuda-toolkit/11.8
openssl/*:jom/1.1.2
qt*:patch-windows/0.1,strawberryperl/5.30.0.1
