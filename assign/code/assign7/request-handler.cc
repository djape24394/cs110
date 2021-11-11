/**
 * File: request-handler.cc
 * ------------------------
 * Provides the implementation for the HTTPRequestHandler class.
 */

#include "request-handler.h"
#include "response.h"
#include "request.h"
#include "client-socket.h"
#include "proxy-exception.h"
#include "ostreamlock.h"
#include <socket++/sockstream.h> // for sockbuf, iosockstream
using namespace std;

HTTPRequestHandler::HTTPRequestHandler()
{
  blackList.addToBlacklist("blocked-domains.txt");
}

void HTTPRequestHandler::serviceRequest(const pair<int, string>& connection){
  sockbuf sb(connection.first);
  iosockstream ss(&sb);
  HTTPRequest request;
  request.ingestRequestLine(ss);
  request.ingestHeader(ss, connection.second);
  request.ingestPayload(ss);
  std::cout << oslock << "Servicing request from the client: " << connection.second << "\n" << osunlock;
  HTTPResponse response;
  if(!blackList.serverIsAllowed(request.getServer()))
  {
    response.setProtocol("HTTP/1.0");
    response.setResponseCode(403);
    response.setPayload("Forbidden Content");
    ss << response;
    ss.flush();
    return;
  }
  mutex &request_mutex = cache.get_request_mutex(request);
  lock_guard<mutex> lg(request_mutex);
  if(!cache.containsCacheEntry(request, response))
  {
    std::cout << oslock << "Connection to server: " << request.getServer() << ":" << request.getPort() << "\n" << osunlock;
    int cs = createClientSocket(request.getServer(), request.getPort());
    if(cs == kClientSocketError)
    {
      throw HTTPBadRequestException("Error connecting to the server " + request.getServer()  + "\n");
    }
    std::cout << oslock << "Connection to server OK\n" << osunlock;
    sockbuf server_buf(cs);
    iosockstream server_socket_stream(&server_buf);
    std::cout << oslock << "Sending request:\n" << osunlock;
    server_socket_stream << request;
    server_socket_stream.flush();
    std::cout << oslock << "Following request sent:\n" << osunlock;
    std::cout << oslock << request << "check return line\n" << osunlock;
    std::cout << oslock << "Ingesting response...\n" << osunlock;
    response.ingestResponseHeader(server_socket_stream);
    if(request.getMethod() != "HEAD")response.ingestPayload(server_socket_stream);
    std::cout << oslock << "Got response:\n" << osunlock;
    if(cache.shouldCache(request, response)) cache.cacheEntry(request, response);
  }
  ss << response;
  ss.flush();
  std::cout << oslock << response << "\n" << osunlock;
}

// the following two methods needs to be completed 
// once you incorporate your HTTPCache into your HTTPRequestHandler
void HTTPRequestHandler::clearCache() {
  cache.clear();
}
void HTTPRequestHandler::setCacheMaxAge(long maxAge) {
  cache.setMaxAge(maxAge);
}
