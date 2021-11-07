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
  std::cout << "Servicing request from the client: " << connection.second << "\n";
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
  if(!cache.containsCacheEntry(request, response))
  {
    std::cout << "Connection to server: " << request.getServer() << ":" << request.getPort() << "\n";
    int cs = createClientSocket(request.getServer(), request.getPort());
    if(cs == kClientSocketError)
    {
      throw HTTPBadRequestException("Error connecting to the server " + request.getServer()  + "\n");
    }
    std::cout << "Connection to server OK\n";
    sockbuf server_buf(cs);
    iosockstream server_socket_stream(&server_buf);
    std::cout << "Sending request:\n";
    server_socket_stream << request;
    server_socket_stream.flush();
    std::cout << "Following request sent:\n";
    std::cout << request << "check return line\n";
    std::cout << "Ingesting response...\n";
    response.ingestResponseHeader(server_socket_stream);
    if(request.getMethod() != "HEAD")response.ingestPayload(server_socket_stream);
    std::cout << "Got response:\n";
    if(cache.shouldCache(request, response)) cache.cacheEntry(request, response);
  }
  ss << response;
  ss.flush();
  std::cout << response << "\n";
}

// the following two methods needs to be completed 
// once you incorporate your HTTPCache into your HTTPRequestHandler
void HTTPRequestHandler::clearCache() {}
void HTTPRequestHandler::setCacheMaxAge(long maxAge) {}
