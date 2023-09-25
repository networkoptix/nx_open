include(common/linux.profile)

[settings]
arch=x86_64

[options]
opencv/*:with_cuda=True
opencv/*:cuda_arch_bin=5.0
qt/*:mysql=True
qt/*:psql=True

[tool_requires]
opencv/*:cuda-toolkit/11.7
