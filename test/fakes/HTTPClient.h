// Stand-in for HTTPClient: replies come from fake_routes, matched by URL substring.
#pragma once
#include <Arduino.h>
#include <WiFiClientSecure.h>

struct FakeReply { int code; std::string body; };
extern std::vector<std::pair<std::string, FakeReply>> fake_routes; // first match wins
extern std::vector<std::string> fake_requests;                     // every URL asked for
extern bool fake_http_begin_ok;

struct HTTPClient
{
  std::string url;
  Stream stream;
  void useHTTP10(bool) {}
  void setTimeout(int) {}
  bool begin(WiFiClientSecure &, const char *u)
  {
    url = u;
    return fake_http_begin_ok;
  }
  int GET()
  {
    fake_requests.push_back(url);
    for (auto &r : fake_routes)
      if (url.find(r.first) != std::string::npos)
      {
        stream.data = r.second.body;
        return r.second.code;
      }
    return -1; // connection refused
  }
  Stream &getStream() { return stream; }
  void end() {}
  static String errorToString(int) { return String("connection refused"); }
};
