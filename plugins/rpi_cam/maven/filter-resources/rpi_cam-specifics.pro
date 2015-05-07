TEMPLATE = lib

INCLUDEPATH +=	$$(environment)/rpi-vc/include \
				$$(environment)/rpi-vc/include/interface \
				$$(environment)/rpi-vc/include/interface/mmal \
				$$(environment)/rpi-vc/include/interface/vcos/pthreads \
				$$(environment)/rpi-vc/include/interface/vmcs_host/linux
QMAKE_INCDIR += $$(environment)/rpi-vc/include \
				$$(environment)/rpi-vc/include/interface \
				$$(environment)/rpi-vc/include/interface/mmal \
				$$(environment)/rpi-vc/include/interface/vcos/pthreads \
				$$(environment)/rpi-vc/include/interface/vmcs_host/linux
QMAKE_LFLAGS += -Wl,-rpath,$$(environment)/rpi-vc/lib
QMAKE_LIBDIR += $$(environment)/rpi-vc/lib
#LIBS -= -lcommon
