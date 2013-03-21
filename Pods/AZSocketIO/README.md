AZSocketIO
==========
AZSocketIO is a socket.io client for iOS. It:

* Supports websockets and xhr-polling transports
* Is about alpha stage
* Is heavily reliant on blocks for it's API
* Has appledocs for all user facing classes
* Welcomes patches and issues

It does not currently support namespacing a socket.

Dependencies
------------
AZSocketIO uses cocoapods, so you shouldn't have to think too much about dependencies, but here they are.

* [SocketRocket](https://github.com/square/SocketRocket)
* [AFNetworking](https://github.com/AFNetworking/AFNetworking)

AZSocketIO uses NSJSONSerialization, so it's iOS 5+.

Usage
-----
``` objective-c
AZSocketIO *socket = [[AZSocketIO alloc] initWithHost:@"localhost" andPort:@"9000"];
[socket setEventRecievedBlock:^(NSString *eventName, id data) {
    NSLog(@"%@ : %@", eventName, data);
}];
[socket connectWithSuccess:^{
	[socket emit:@"Send Me Data" args:@"cows" error:nil];
} andFailure:^(NSError *error) {
    NSLog(@"Boo: %@", error);
}];
```

Contact
-------
Pat Shields

* http://github.com/pashields
* http://twitter.com/whatidoissecret

License
-------
Apache 2.0