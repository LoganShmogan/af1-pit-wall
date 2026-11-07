// Stand-in for HTTPClient: replies come from fake_routes, matched by URL substring.
#pragma once
#include <Arduino.h>
#include <WiFiClientSecure.h>

struct FakeReply { int code; std::string body; };
extern std::vector<std::pair<std::string, FakeReply>> fake_routes; // first match wins
extern std::vector<std::string> fake_requests;                     // every URL asked for
extern bool fake_http_begin_ok;
extern void (*fake_on_get)(); // called during GET(), e.g. to change state mid-download
extern bool fake_gzip;        // replies arrive gzip-compressed (as OpenF1 sends them)
extern int fake_write_error;  // < 0: writeToStream() fails with this code
extern std::string fake_accept_encoding;
std::string fakeGzip(const std::string &plain);
#define HTTPC_ERROR_READ_TIMEOUT (-11)

struct HTTPClient
{
  std::string url;
  Stream stream;
  void setAcceptEncoding(const String &e) { fake_accept_encoding = e; }
  void setTimeout(int) {}
  bool begin(WiFiClientSecure &, const char *u)
  {
    url = u;
    return fake_http_begin_ok;
  }
  int GET()
  {
    fake_requests.push_back(url);
    if (fake_on_get) fake_on_get();
    for (auto &r : fake_routes)
      if (url.find(r.first) != std::string::npos)
      {
        stream.data = r.second.body;
        return r.second.code;
      }
    return -1; // connection refused
  }
  Stream &getStream() { return stream; }
  int writeToStream(Stream *out)
  {
    if (fake_write_error) return fake_write_error;
    std::string body = fake_gzip ? fakeGzip(stream.data) : stream.data;
    size_t half = body.size() / 2; // arrives in two pieces, like a real download
    out->write((const uint8_t *)body.data(), half);
    out->write((const uint8_t *)body.data() + half, body.size() - half);
    return body.size();
  }
  void end() {}
  static String errorToString(int) { return String("connection refused"); }
};
