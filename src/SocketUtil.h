#pragma once
#include <cstdlib>
#include <vector>
#include <sys/socket.h>
#include <string>
// Log function
extern void Log(std::string s);
// Socket util function
// The socket contains 2 mode
// Server & Clinet
// Server mode:
//     Set up using default configuration
//     Automatically connect new tcp request and capture a handler
//     Using select to achieve high-performance processing
struct IServerComponent {
    // This function will be called when the channel have some data to read
    // The buf is not thread-safe
    virtual void readFromSocket(const char *buf, int size) = 0;
    // This function will be called when channal is able to write
    // If this return is true, there are something need to write again
    virtual bool writeToSocket(int socketFD) = 0;
    // This function tells sever some data is pending
    virtual bool pending() = 0;
    virtual ~IServerComponent() {};
};


struct IServerComponentBuilder {
    // This call will return a new instance of IServerComponent
    virtual IServerComponent* build() = 0;
    // This function is called when a socket is close
    virtual void socketClose(IServerComponent *ptr) = 0;
};

class BaseServerComponent;
class SocketServerUtil {
public:
    // Settings
    static const int max_connection = SOMAXCONN;

    SocketServerUtil() : builder(NULL) {}
    // Start server
    void start(int portnumber);
    // Set server component
    void setServerComponent(IServerComponentBuilder *builder);
private:
    IServerComponentBuilder *builder;
    // Close socket
    friend class BaseServerComponent;
    void closeSocket(IServerComponent *handler);
    // Server function
    void init();
    void loop();
    // Server configure
    int portnumber;
    int sock;
    std::vector<IServerComponent*> closedHandler;
};

// Base of all server component
// provide closeSocket function
class BaseServerComponent : public IServerComponent{
public:
    BaseServerComponent(SocketServerUtil *server) : server(server) {}
    void closeSocket();
    virtual ~BaseServerComponent() {};
private:
    SocketServerUtil *server;
};

// Base of all Server Builder
struct BaseServerComponentBuilder : public IServerComponentBuilder{
    BaseServerComponentBuilder(SocketServerUtil *server) : server(server) {}
protected:
    SocketServerUtil *server;
};

// The socket util provide a native builder
template <class T> class SimpleServerComponentBuilder: public BaseServerComponentBuilder {
    public:
	// Initial the base class
	SimpleServerComponentBuilder(SocketServerUtil *server) : BaseServerComponentBuilder(server) {}
	// This function is called when the server require to instance a new ServerComponent object
	virtual IServerComponent * build()
	{
	    IServerComponent *component = new T(server);
	    return component;
	}
	// This function is called when the server close a socket
	// Free the memeory of ServerComponent
	virtual void socketClose(IServerComponent *ptr) 
	{
	    if (ptr) 
	    {
		delete ptr;
	    }
	}
};

