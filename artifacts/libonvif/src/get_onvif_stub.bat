set GSOAP_HOME=F:\Soft\gsoap\gsoap-2.8.8

@rem %GSOAP_HOME%\gsoap\bin\win32\wsdl2h.exe -t typemap.dat -o onvif.h http://www.w3.org/2006/03/addressing/ws-addr.xsd http://docs.oasis-open.org/wsn/bw-2.wsdl http://www.onvif.org/onvif/ver10/device/wsdl/devicemgmt.wsdl http://www.onvif.org/onvif/ver10/event/wsdl/event.wsdl http://www.onvif.org/onvif/ver20/imaging/wsdl/imaging.wsdl http://www.onvif.org/onvif/ver10/media/wsdl/media.wsdl http://www.onvif.org/onvif/ver20/ptz/wsdl/ptz.wsdl http://www.onvif.org/ver10/deviceio.wsdl
%GSOAP_HOME%\gsoap\bin\win32\soapcpp2.exe -wxj2 onvif.h -I%GSOAP_HOME%\gsoap\import\ -I%GSOAP_HOME%\gsoap\
