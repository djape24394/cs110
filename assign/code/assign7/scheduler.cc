/**
 * File: scheduler.cc
 * ------------------
 * Presents the implementation of the HTTPProxyScheduler class.
 */

#include "scheduler.h"
#include <utility>
using namespace std;

HTTPProxyScheduler::HTTPProxyScheduler():threadPool((size_t)64u){}

void HTTPProxyScheduler::scheduleRequest(int clientfd, const string& clientIPAddress) {
  threadPool.schedule([this, clientfd, &clientIPAddress]()
  { 
    try
    {
      requestHandler.serviceRequest(make_pair(clientfd, clientIPAddress));    
    }
    catch(const std::exception& e)
    {
      std::cerr << e.what() << '\n';
    }
  });
}

void HTTPProxyScheduler::setProxy(const std::string& server, unsigned short port)
{
  requestHandler.setProxy(server, port);
}
