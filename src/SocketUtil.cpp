#include "SocketUtil.h"
#include <string>
#include <memory.h>
#include <algorithm>
#include <iostream>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <stdexcept>
#include <vector>
#include <unordered_map>
#include <time.h>
using namespace std;
const bool debug = true;
string int_tostring(int num)
{
	return std::to_string((long long) num);
}
void Log(string s)
{
    if (debug) {
	time_t now = time(0);
	struct tm ts;
	ts = *localtime(&now);
	char buf[30];
	strftime(buf, sizeof(buf), "[%Y%m%d-%T]: ", &ts);
	cout << buf << s << endl;
    }
}

using namespace std;

void BaseServerComponent::closeSocket() 
{
    if (server)
	server->closeSocket(this);
}

void SocketServerUtil::start(int portnumber)
{
    this->portnumber = portnumber;
    init();
    loop();
}

void SocketServerUtil::init()
{
    struct sockaddr_in sin;
    sock = socket ( PF_INET, SOCK_STREAM, IPPROTO_TCP );
    if (sock < 0) 
    {
	throw runtime_error("Cannot open tcp socket");
    }
    int optval = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0)
    {
	throw runtime_error("Cannot set socket to reuse addr");
    }
    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = INADDR_ANY;
    sin.sin_port = htons(portnumber);
    /* Bind server socket */
    if (bind(sock, (struct sockaddr *) &sin, sizeof(sin)) < 0) 
    {
	throw runtime_error("Cannot bind");
    }
    if (listen(sock, SocketServerUtil::max_connection) < 0)
    {
	throw runtime_error("Cannot listen");
    }
    Log(string() + "Success initialize server at port" + int_tostring(portnumber));
}

void SocketServerUtil::closeSocket(IServerComponent *component)
{
    closedHandler.push_back(component);
}

void SocketServerUtil::setServerComponent(IServerComponentBuilder *builder)
{
    this->builder = builder;
}

    struct SocketInfo {
	int socketNo;
	IServerComponent *component;
    };

void SocketServerUtil::loop()
{
    fd_set read_set, write_set;
    vector<SocketInfo> socketLists;
    unordered_map<IServerComponent*, int> hashMap;
    int max_socket = 0;
    // Socket select interval
    struct timeval time_out;
    int max_buf_len = 500;
    char buf[max_buf_len];

    while (true) {
	FD_ZERO(&read_set);
	FD_ZERO(&write_set);
	// Set listening socket
	FD_SET(sock, &read_set);
	max_socket = sock;
	for (int i = 0 ; i != socketLists.size() ; ++i) 
	{
	    int socketNo = socketLists[i].socketNo;
	    FD_SET(socketNo, &read_set);
	    if (socketLists[i].component->pending()) {
		FD_SET(socketNo, &write_set);
	    }
	    max_socket = std::max(max_socket, socketNo);
	}

	// Select socket
	time_out.tv_usec = 100000;
	time_out.tv_sec = 0;
	int select_retval = select(max_socket + 1, &read_set, &write_set, NULL, &time_out);
	if (select_retval < 0)
	{
	    throw runtime_error("Select failed");
	}
	if (select_retval == 0)
	{
	    continue;
	}
	if (select_retval > 0)
	{
	    // Some new data in or can write some data
	    // Check the server socket
	    if (FD_ISSET( sock, &read_set)) 
	    {
		struct sockaddr_in addr;
		socklen_t len = sizeof(sockaddr_in);
		int new_sock = accept(sock, (struct sockaddr *) &addr, &len);
		Log("Accept Connection sock number = " + int_tostring(new_sock));
		if (new_sock < 0)
		{
		    throw runtime_error("cannot accept new socket");
		}
		if (fcntl(new_sock, F_SETFL, O_NONBLOCK) < 0) 
		{
		    throw runtime_error("cannot set socket to nonblock");
		}
		// Create new socket
		SocketInfo info;
		info.socketNo = new_sock;
		info.component = builder->build();
		socketLists.push_back(info);
	    }
	    // Check other socket
	    for (int i = socketLists.size() - 1; i >= 0 ; --i)
	    {
		SocketInfo &info = socketLists[i];
		if (FD_ISSET(info.socketNo, &read_set))
		{
		    int count = recv(info.socketNo, buf, max_buf_len, 0);
		    // Log(string("receive data : ") + string(buf, count));
		    if (count < 0)
		    {
			throw runtime_error("cannot read socket");
		    }
		    if (count == 0)
		    {
			// Clean up
			socketLists.erase(socketLists.begin() + i);
			continue;
		    }
		    if (count > 0)
		    {
			info.component->readFromSocket(buf, count);
		    }
		}
		if (FD_ISSET(info.socketNo, &write_set))
		{
		    info.component->writeToSocket(info.socketNo);
		}
	    }
	}
	// Remove useless socket
	int first, result;
	first = result = 0;
	for (int i = 0 ; i != closedHandler.size() ; ++i)
	    hashMap[closedHandler[i]] = 1;
	while (first < socketLists.size()) {
	    if (hashMap.find(socketLists[first].component) == hashMap.end()) {
		socketLists[result] = socketLists[first];
		result++;
	    } else {
		// Free socket
		Log("Free socket " + int_tostring(socketLists[first].socketNo));
		close(socketLists[first].socketNo);
		builder->socketClose(socketLists[first].component);
	    }
	    first++;
	}
	closedHandler.clear();
	hashMap.clear();
	socketLists.resize(result);
    }
}
