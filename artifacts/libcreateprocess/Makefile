
CXX             = /usr/local/codesourcery/arm-2011.09/bin/arm-none-linux-gnueabi-g++
CXXFLAGS        = -ggdb3 -fPIC
LFLAGS          = -shared -fPIC

TARGET          = libcreateprocess.so


$(TARGET): linux_create_process.o
		$(CXX) $(LFLAGS) -o $(TARGET) linux_create_process.o

clean:
		rm -rf $(TARGET)
		rm -rf linux_create_process.o

install:
		cp ./create_process.h ~/develop/netoptix_vms.arm/build_environment/target-isd/include/libcp/
		cp ./libcreateprocess.so ~/develop/netoptix_vms.arm/build_environment/target-isd/lib/debug/
		md5sum ./libcreateprocess.so > ~/develop/netoptix_vms.arm/build_environment/target-isd/lib/debug/libcreateprocess.so.md5
		cp ./libcreateprocess.so ~/develop/netoptix_vms.arm/build_environment/target-isd/lib/release/
		md5sum ./libcreateprocess.so > ~/develop/netoptix_vms.arm/build_environment/target-isd/lib/release/libcreateprocess.so.md5

linux_create_process.o: linux_create_process.cpp
		$(CXX) $(CXXFLAGS) -o linux_create_process.o -c ./linux_create_process.cpp
