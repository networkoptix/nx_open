TEMPLATE = lib

unix
{
  QMAKE_CXXFLAGS += -Wno-unused-function -DDTVAPI_V15
  QMAKE_CFLAGS += -DDTVAPI_V15
}

LIBS -= -lcommon
