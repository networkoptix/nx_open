NX_BUILDENV=`pwd`/../build-environment/x86/build/
CXX=g++
AR=ar
ARFLAGS=rcs
INCLUDES=-I$(NX_BUILDENV)/include/
DEFINES=-DWITH_OPENSSL -DWITH_PURE_VIRTUAL

CXXFLAGS_DEBUG=-fPIC -ggdb3
DEBUG_OBJECTS_DIR=.build/debug/
debug_objects := $(patsubst src/%.cpp,$(DEBUG_OBJECTS_DIR)/%.o,$(wildcard src/*.cpp)) $(patsubst src/%.c,$(DEBUG_OBJECTS_DIR)/%.o,$(wildcard src/*.c))
DEBUG_TARGET=$(DEBUG_OBJECTS_DIR)/libonvif.a

CXXFLAGS_RELEASE=-fPIC -o2
RELEASE_OBJECTS_DIR=.build/release/
release_objects := $(patsubst src/%.cpp,$(RELEASE_OBJECTS_DIR)/%.o,$(wildcard src/*.cpp)) $(patsubst src/%.c,$(RELEASE_OBJECTS_DIR)/%.o,$(wildcard src/*.c))
RELEASE_TARGET=$(RELEASE_OBJECTS_DIR)/libonvif.a


$(DEBUG_OBJECTS_DIR): 
		mkdir -p $(DEBUG_OBJECTS_DIR)
$(DEBUG_OBJECTS_DIR)/%.o: src/%.c
		$(CXX) $(CXXFLAGS_DEBUG) $(DEFINES) $(INCLUDES) -c -o $@ $^
$(DEBUG_OBJECTS_DIR)/%.o: src/%.cpp
		$(CXX) $(CXXFLAGS_DEBUG) $(DEFINES) $(INCLUDES) -c -o $@ $^
$(DEBUG_TARGET): $(DEBUG_OBJECTS_DIR) $(debug_objects)
		$(AR) $(ARFLAGS) $(DEBUG_TARGET) $(debug_objects)
debug: $(DEBUG_TARGET)


$(RELEASE_OBJECTS_DIR): 
		mkdir -p $(RELEASE_OBJECTS_DIR)
$(RELEASE_OBJECTS_DIR)/%.o: src/%.c
		$(CXX) $(CXXFLAGS_RELEASE) $(DEFINES) $(INCLUDES) -c -o $@ $^
$(RELEASE_OBJECTS_DIR)/%.o: src/%.cpp
		$(CXX) $(CXXFLAGS_RELEASE) $(DEFINES) $(INCLUDES) -c -o $@ $^
$(RELEASE_TARGET): $(RELEASE_OBJECTS_DIR) $(release_objects)
		$(AR) $(ARFLAGS) $(RELEASE_TARGET) $(release_objects)
release: $(RELEASE_TARGET)


all: $(TARGET)

clean:
		rm -rf $(DEBUG_OBJECTS_DIR)
		rm -rf $(RELEASE_OBJECTS_DIR)
