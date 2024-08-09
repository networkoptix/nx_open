include(common/linux.profile)

[settings]
arch=x86_64
opencv*:build_type=RelWithDebInfo

[options]
opencv-static/*:cuda_arch_bin=5.0
opencv-static/*:cudawarping=True
opencv-static/*:with_cuda=True
qt/*:mysql=True
qt/*:psql=True

[tool_requires]
opencv-static/*:cuda-toolkit/12.5.1

[conf]
icu/*:tools.build:exelinkflags=["-static-libstdc++", "-static-libgcc"]
