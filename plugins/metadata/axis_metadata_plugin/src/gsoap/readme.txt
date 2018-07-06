How to genererate both event and action soap source code.
wsdl2h -c++11 -IWS -tWS/typename.dat -d -o generated_with_wsdl2h/axis_soap_event_action http://www.axis.com/vapix/ws/event1/EventService.wsdl http://www.axis.com/vapix/ws/action1/ActionService.wsdl http://www.w3.org/2006/03/addressing/ws-addr.xsd
soapcpp2 -c++11 -j -C -y -I import -d generated_with_soapcpp2 generated_with_wsdl2h/axis_soap_event_action

//wsdl2h-t -IWS -tWS/typename.dat
// http://www.onvif.org/ver10/events/wsdl/event.wsdl

How to genererate event soap source code.
wsdl2h -c++11 -d -o generated_with_wsdl2h/axis_soap_event http://www.axis.com/vapix/ws/event1/EventService.wsdl http://www.w3.org/2006/03/addressing/ws-addr.xsd
soapcpp2 -c++11 -j -C -y -x -I import -d generated_with_soapcpp2 generated_with_wsdl2h/axis_soap_event


How to genererate action soap source code.
wsdl2h -c++11 -d -o generated_with_wsdl2h/axis_soap_action http://www.axis.com/vapix/ws/action1/ActionService.wsdl http://www.w3.org/2006/03/addressing/ws-addr.xsd
soapcpp2 -c++11 -j -C -y -x -I import -d generated_with_soapcpp2 generated_with_wsdl2h/axis_soap_action


wsdl2h keys:
-c++11	generate C++11 source code
-d      use DOM to populate xs:any, xs:anyType, and xs:anyAttribute
-o		output file

soapcpp2 keys:
-c++11  generate C++11 source code
//-i      generate C++ service proxies and objects inherited from soap struct
-j      generate C++ service proxies and objects share a soap struct
-C		generate client-side code only
//-x		don't generate sample XML message files
-I      use import directory
-y	c++ code in example XML comments


 