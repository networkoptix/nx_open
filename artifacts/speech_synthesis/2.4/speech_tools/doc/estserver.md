Client-Server Mechanisms {#estserver}
==========================

[TOC]


The C++ class \ref EST_Server  provides the core
mechanisms required for simple client-server applications. It is
currently used to implement the 
`fringe_client`
program and client-server mechanisms for SIOD. It is planned to
use it to re-implement the festival client-server mechanism.

Servers have types and names. When a server is started it
records it's type, name and location in a `services' file. When
a client wishes to connect to a server it looks for it's
location in that file by giving a name and type. 

Once connected, a client must present a magic cookie to the
server as a simple form of authentication. Once authenticated
the client sends requests, consisting of a package name,
operation name and a set of arguments, to the server. The server
responds with an error report or a sequence of values.

An instance of \ref EST_Server  embodies each
side of the client-server relationship. In the server an
instance of \ref EST_Server  is created and
told how to process requests from clients, a call to the
EST_Server::run() method then starts the
server. In a client an instance of \ref EST_Server 
represents the server, and calls to the 
EST_Server::execute() method send
requests to the server.

# The Services Table {#estserverservicetable}

The first problem which needs to be addressed by any
client-server system is how the client finds the
server. Servers based on \ref EST_Server 
handle this problem by writing a record into a file giving
their name, type and location. Clients can then look servers
up by namd and type.

By default the file `.estServices` is used
for this purpose, meaning that each user has their own list of
servers. An alternative file could be specified to record
public services.

The services table also provides a simple authorisation
mechanism. Each server records a random string in the table,
and clients must send this string before making any
requests. Thus people who can't read the services table can't
make requests of the server, and the file permissions on the
services table can be used to control access to the server.
\par Important:

This `magic cookie' authorisation scheme is not very
secure. The cookie is sent as plain text over the
network and so anyone who can snoop on the network can
break the security. 


A more secure `challange-responce' authorisation scheme
should be implemented.



The in-file format of the services table is based on the
Java properties file format. A typical file might look as
follows:

@code
	  #Services
	  fringe.type=fringe
	  fringe.host=foo.bar.com
	  fringe.cookie=511341634
	  fringe.port=56362
	  fringe.address=123.456.789.654
	  siod.type=siod
	  siod.cookie=492588950
	  siod.host=foo.bar.com
	  siod.address=123.456.789.654
	  siod.port=56382
	  labeling.type=fringe
	  labeling.host=foo.bar.com
	  labeling.cookie=511341634
	  labeling.port=56362
	  labeling.address=123.456.789.654
@endcode

This file lists three services, a
`fringe` server with the default
name of `fringe`, a scheme interpreter
running as a server, also with the default name, and a second
`fringe` server named `labeling`.


The programing interface to the services table is provided by
the \ref EST_ServiceTable class. 


# Writing Clients and Servers {#estserverwritingclientserver}

If a service type (that is a sub-class of
\ref EST_Server ) has already been defined
for the job you need to do, creating clients and servers is
quite straight forward. For this section I will use the
\ref EST_SiodServer  class, which defines a
simple scheme execution service service, as an example.

## A Simple Server

To run a siod server we have to read the server table,
create the server object and update the table, then start the
service running.

First we read the default service table.

@code{.cpp}
 EST_ServiceTable::read();
@endcode

Now we create the new scheme service called "mySiod". The
`sm_sequential` parameter to the \ref Mode
server constructor tells the server to deal with one client
at a time. The `NULL` turns off trace
output, replace this with `&amp;cout` to see
what the server is doing.

@code{.cpp}
	    EST_SiodServer *server 
		= new EST_SiodServer(EST_Server::sm_sequential,
				     "mySiod",
				     NULL);
@endcode

Write the table back out so clients can find us.

@code{.cpp}
	    EST_ServiceTable::write();
@endcode

Create the object which handles the client requests. The
`handler` object actually does the work
the client requests.  \ref EST_SiodServer 
provides the obvious default handler (it executes the scheme
code and returns the results), so we use that.
	  
@code{.cpp}
	    EST_SiodServer::RequestHandler handler;
@endcode

Finally, start the service. This call never returns.

@code{.cpp}
	    server->run(handler);
@endcode

## A Simple Client {#simple-client}

A client is created by reading the service table, and then
asking for a server by name. Again the `NULL` means `no trace output'.

@code{.cpp}
	    EST_ServiceTable::read();
	    
	     EST_SiodServer *server 
		= new EST_SiodServer("mySiod", NULL);
@endcode

Now we have a representation of the server we must connect
before we can do anything. We can connect and dissconnect a
server object any number of times over it's life. This may
or may not have some meaning to the server. The return value
of the connect operation tells us if we managed to connect.

@code{.cpp}
	    if (server->connect() != connect_ok)
		EST_sys_error("Error Connecting");
@endcode

Once we are connected we can send requests to the
server. The siod server executes scheme for us, assume that
the function \ref get_sexp() returns something
we want evaluated.

@code{.cpp}
	    LISP expression = get_sexp();
@endcode

We pass arguments to requests in an \ref Args 
structure, a special type of
\ref EST_Features . The siod server wants
the expression to execute as the value of
`sexp`.

@code{.cpp}
	    EST_SiodServer::Args args;
	    args.set_val("sexp", est_val(expression));
@endcode

As in the server, the behaviour of the client is defined by
a `handler' object. The handler
\ref EST_SiodServer  defines for us does
nothing with the result, leaving it for us to deal with in
the \ref EST_Features  structure
`handler.res`. Again this is good enough
for us.

@code{.cpp}
	    EST_SiodServer::ResultHandler handler;
@endcode

Finally we are ready to send the request to the server. The
siod server provides only one operation, called
`"eval"` in package
`"scheme"`, this is the evaluate-expression
operation we want. The return value of
\ref execute() is true of everything goes
OK, false for an error. For an error the message is the
value of `"ERROR"`.
	  
@code{.cpp}
	    if (!server->execute("scheme", "eval", args, handler))
		EST_error("error from siod server '%s'",
			  (const char *)handler.res.String("ERROR"));
@endcode

Now we can get the result of the evaluation, it is returned
as the value of `"sexp"`. 

@code{.cpp}
	    LISP result = scheme(handler.res.Val("sexp"));
@endcode

Although this may seem a lot of work just to evaluate one
expression, once a connection is established, only the three
steps set arguments, execute, extract results need to be
done for each request. So the following would be the code
for a single request:

@code{.cpp}
	    args.set_val("sexp", est_val(expression));
	    if (!server->execute("scheme", "eval", args, handler))
		    [handle error]
	    LISP result = scheme(handler.res.Val("sexp"));
@endcode

## A Specialised Server {#estserverspecializedserver}

If you need to create a server similar to an existing one
but which handles requests slightly differently, all you
need to do is define your own
\ref RequestHandler  class. This class has
a member function called 
RequestHandler::process() which does the work.


Here is a variant on the siod server which handles a new
operation `"print"` which evaluates an
expression and prints the result to standard output as well
as retruning it. (In this example some details of error
catching and so on necessary for dealing with scheme are
omitted so as not to obscure the main points).


First we define the handler class. It is a sub-class of the
default handler for siod servers.

@code{.cpp}
	    class MyRequestHandler : public EST_SiodServer::RequestHandler
	    {
	    public:
	    virtual EST_String process(void);
	    };
@endcode

Now, we define the processing method. For any operation
other than `"print"` we call the default
siod handler. (\ref leval and
\ref lprint are functions provided by the
siod interpreter).

@code{.cpp}
	    EST_String MyRequestHandler::process(void)
	    {
	    if (operation == "print")
	        {
	        // Get the expression.
	        LISP sexp = scheme(args.Val("sexp"));
	        
	        // Evaluate it.
	        LISP result = leval(sexp, current_env);

	        // Print it.
	        lprint(result);
	        
	        // Return it.
	        res.set_val("sexp", est_val(result));
	        return "";
	        }
	    else
	        // Let the default handler deal with other operations.
	        return EST_SiodServer::RequestHandler::process();
	    }
@endcode

And now we can start a server which understands the new
operation.

@code{.cpp}
	    MyRequestHandler handler;
	    server->run(handler);
@endcode

## A Client Which Handles Multiple Results {#estserverclientmultiplereq}

Servers have the option to return more than one value for a
single request. This can be used to return the results of a
request a piece at a time as they become available, for
instance *festival* returns a waveform for each sentence in
a piece of text it is given to synthesise.


Clearly a simple client of the kind described
\link simple-client above \endlink which gets the
result of a request as a result of the call to
EST_SiodServer::execute() can't handle
multiple results of this kind. This is what the handler
object is for.

I'll asume we need a client to deal with a variant on the
normal siod sever which returns multiple values, say it
evaluates the expression in each of a number of environments
and returns each result separately. I'll also assume that
the work to be done for each result is defined by the fucntion
\ref deal_with_result().


Most of the client will be the same as for
\link simple-client above \endlink,
the exception is that we use our own result handler rather
than the default one.

@code{.cpp}
	    class MyResultHandler : public EST_SiodServer::ResultHandler
	    {
	    public:
	    virtual void process(void);
	    };
@endcode

As for the server's request handler, the behaviour of the
result handler is defined by the 
process() method of the handler.

@code{.cpp}
	    EST_String MyResultHandler::process(void)
	    {
	    // Get the result.
	    LISP result = scheme(handler.res.Val("sexp"));

	    // And deal with it.
	    deal_with_result(result);
	    }
@endcode

With this definition in place we can make requests to the
server as follows.

@code{.cpp}
	    MyResultHandler handler;
	    if (!server->execute("scheme", "multi-eval", args, handler))
		[handle errors]
@endcode

The \ref deal_with_result() function will be
called on each result which is returned. If anything special
needs to be done with the final value, it can be done after
the call to EST_SiodServer::execute()
as in the simple client example.

# Creating a new Service

Not written

## Commands

Not written

## Results

Not written

# The Network Protocol

Not written
