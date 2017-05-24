This directory contains sources of "tegra_video" - a dynamic library "libtegra_video" intended for NVidia Tegra (Tx1) which performs hardware video decoding and neural network processing, and an executable "video_dec_gie" which allows to run in on Tx1 for testing/debugging.

The sources are based on examples provided by NVidia as "tegra_multimedia_api", mainly on example
"samples/04_video_dec_gie". The original source code is preserved, and its main() can be accessed
running the executable with the key "--original". The new functionality is packed into the library
"libtegra_video.so" which is compiled from these sources together with the original executable. For
debug/testing, this functionality can be accessed running the executable with the key "--nx".

This artifact is intended to be compiled on Tx1:
cd samples/04_video_dec_gie && make

The build results will be found in "samples/04_video_dec_gie":
video_dec_gie (executable)
libtegra_video.so

NOTE: The file "README" in this folder is the original NVidia's README for "tegra_multimedia_api".
