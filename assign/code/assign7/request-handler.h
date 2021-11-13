/**
 * File: request-handler.h
 * -----------------------
 * Defines the HTTPRequestHandler class, which fully proxies and
 * services a single client request.  
 */

#ifndef _request_handler_
#define _request_handler_

#include <utility>
#include <string>
#include "blacklist.h"
#include "cache.h"

class HTTPRequestHandler {
  HTTPBlacklist blackList;
  HTTPCache cache;
  bool usingProxy;
  std::string proxyServer;
  unsigned short proxyPortNumber;
 public:
  HTTPRequestHandler();
  void serviceRequest(const std::pair<int, std::string>& connection);
  void clearCache();
  void setCacheMaxAge(long maxAge);
  void setProxy(const std::string& server, unsigned short port);
};

#endif
