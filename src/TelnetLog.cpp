#include "TelnetLog.h"
// Define the global instance
TelnetLogger Log;
TelnetLogger::TelnetLogger() : telnetServer(23) {
}
void TelnetLogger::begin(unsigned long baud) {
    Serial.begin(baud);
}
void TelnetLogger::startServer() {
    telnetServer.begin();
    telnetServer.setNoDelay(true);
    Serial.println("Telnet server started on port 23");
}
void TelnetLogger::handleClient() {
    if (telnetServer.hasClient()) {
        if (!telnetClient || !telnetClient.connected()) {
            if (telnetClient) telnetClient.stop();
            telnetClient = telnetServer.available();
            Serial.println("New telnet client connected");
            telnetClient.println("=== ESP32 Flight Monitor Telnet Log ===");
        } else {
            WiFiClient newClient = telnetServer.available();
            newClient.stop(); // Reject multiple connections
        }
    }
}
size_t TelnetLogger::write(uint8_t c) {
    size_t result = Serial.write(c);
    if (telnetClient && telnetClient.connected()) {
        telnetClient.write(c);
    }
    return result;
}
size_t TelnetLogger::write(const uint8_t *buffer, size_t size) {
    size_t result = Serial.write(buffer, size);
    if (telnetClient && telnetClient.connected()) {
        telnetClient.write(buffer, size);
    }
    return result;
}
int TelnetLogger::printf(const char *format, ...) {
    char loc_buf[64];
    char * temp = loc_buf;
    va_list arg;
    va_list copy;
    va_start(arg, format);
    va_copy(copy, arg);
    int len = vsnprintf(temp, sizeof(loc_buf), format, copy);
    va_end(copy);
    if(len < 0) {
        va_end(arg);
        return 0;
    }
    if(len >= (int)sizeof(loc_buf)){
        temp = (char*)malloc(len+1);
        if(temp == NULL) {
            va_end(arg);
            return 0;
        }
        vsnprintf(temp, len+1, format, arg);
    }
    va_end(arg);
    int ret = write((uint8_t*)temp, len);
    if(temp != loc_buf){
        free(temp);
    }
    return ret;
}
