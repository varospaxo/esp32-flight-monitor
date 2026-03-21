#ifndef TELNETLOG_H
#define TELNETLOG_H
#include <Arduino.h>
#include <WiFiServer.h>
#include <WiFiClient.h>
class TelnetLogger : public Print {
public:
    TelnetLogger();
    void begin(unsigned long baud);
    void startServer();
    void handleClient();
    virtual size_t write(uint8_t c) override;
    virtual size_t write(const uint8_t *buffer, size_t size) override;
    // Add explicitly to avoid ambiguity issues with some Print implementations
    int printf(const char *format, ...);
private:
    WiFiServer telnetServer;
    WiFiClient telnetClient;
};
#endif // TELNETLOG_H
