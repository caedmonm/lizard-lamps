#pragma once
#include <WebServer.h>

// Arduino WebServer discards socket write results. Stop after a failed write so
// a streaming response cannot retry every remaining chunk on a stalled socket.
class DeviceHttpServer : public WebServer {
  static constexpr uint32_t ResponseBudgetMs = 8000;
  uint32_t responseStarted = 0;
  bool started = false, failed = false;

  void abortResponse() {
    if (!failed) Serial.println("HTTP response aborted: disconnected, stalled or too slow");
    failed = true;
    client().stop();
  }
protected:
  size_t _currentClientWrite(const char* bytes, size_t length) override {
    if (failed || !length) return 0;
    if (!started) { responseStarted = millis(); started = true; }
    if (!client().connected() || uint32_t(millis() - responseStarted) >= ResponseBudgetMs) {
      abortResponse(); return 0;
    }
    const size_t written = WebServer::_currentClientWrite(bytes, length);
    // The library call itself can block; this budget is checked between writes.
    if (written != length || uint32_t(millis() - responseStarted) >= ResponseBudgetMs)
      abortResponse();
    return written;
  }
  size_t _currentClientWrite_P(PGM_P bytes, size_t length) override {
    // On ESP32 program-memory data is memory-mapped; NetworkClient::write_P also
    // delegates to its regular write method.
    return _currentClientWrite(bytes, length);
  }
public:
  using WebServer::WebServer;
  bool responseFailed() const { return failed; }
  void handleClient() {
    started = failed = false;
    WebServer::handleClient();
  }
};
