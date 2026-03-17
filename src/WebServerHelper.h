#ifndef WEB_SERVER_HELPER_H
#define WEB_SERVER_HELPER_H

#include <ESPAsyncWebServer.h>

extern AsyncWebServer server;

void setupServer();
bool checkAuth(AsyncWebServerRequest* req);

#endif // WEB_SERVER_HELPER_H
