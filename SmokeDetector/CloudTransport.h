#pragma once

// Avoid Blynk's Wi-Fi helper: it waits indefinitely for SNTP inside connect().
#include <ESP8266WiFi.h>
#include <WiFiClientSecureBearSSL.h>
#include <BlynkApiArduino.h>
#include <Blynk/BlynkProtocol.h>
#include <Adapters/BlynkArduinoClient.h>
#include "Settings.h"

class BoundedTlsClient : public BearSSL::WiFiClientSecureCtx {
 public:
  int connect(const char* hostname, uint16_t port) override {
    IPAddress address;
    if (WiFi.status() != WL_CONNECTED ||
        !WiFi.hostByName(hostname, address, smoke::kNetworkTimeoutMs)) return 0;
    setTimeout(smoke::kNetworkTimeoutMs);
    if (!WiFiClient::connect(address, port)) return 0;
    // Retain SNI and hostname verification. BearSSL 3.1.2 bounds its handshake
    // at 15 seconds; recurrent sensor servicing runs during network yields.
    const int result = _connectSSL(hostname);
    setTimeout(smoke::kNetworkTimeoutMs);
    return result;
  }
  int connect(IPAddress, uint16_t) override { return 0; }
};

using CloudTransport = BlynkArduinoClientGen<BoundedTlsClient>;
class CloudClient : public BlynkProtocol<CloudTransport> {
 public:
  explicit CloudClient(CloudTransport& transport) : BlynkProtocol(transport) {}
  void config(const char* token) {
    begin(token);
    conn.begin("blynk.cloud", 443);
  }
};

static const char kCloudRootCa[] PROGMEM =
#include <certs/letsencrypt_pem.h>
;
