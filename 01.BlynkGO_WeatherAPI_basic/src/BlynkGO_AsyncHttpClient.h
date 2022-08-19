#ifndef __BLYNKGO_ASYNC_HTTPCLIENT_H__
#define __BLYNKGO_ASYNC_HTTPCLIENT_H__

#include <Arduino.h>
#include <WiFi.h>
#include "blynkgo_common_wifi.h"
#include <vector>
#include <FS.h>
#include <SPIFFS.h>
#include <SD.h>

#define BLYNKGO_ASYNCH_HTTPCLIENT_VERSION           "0.3 Beta"

static const int HTTP_ERROR_CONNECTION_FAILED = -1;
static const int HTTP_ERROR_TIMED_OUT = -2;
static const int HTTP_ERROR_INVALID_RESPONSE = -3;

enum {
  ASYNC_HTTP_GET,
  ASYNC_HTTP_POST
};
typedef uint8_t HttpMethod;

enum  {
  ASYNC_STATE_IDLE,
  ASYNC_STATE_START,
  ASYNC_STATE_READING_STATUS_LINE,
  ASYNC_STATE_READING_HEADERS,
  ASYNC_STATE_READING_BODY
};
typedef uint8_t HttpState;

struct Request {
  IPAddress address;
  String    hostname;
  uint16_t  port;
  String    path;
  HttpMethod method;
  String    contentType;
  char     *data;
  size_t    dataSize;
  bool      is_ssl;
};
struct HttpHeader {
  String key;
  String value;
};

struct Response {
  uint8_t httpCode      = 0;
  String  statusText    = "";
  String  contentType   = "";
  size_t  contentLength = 0;
  std::vector<HttpHeader> headers;
//  String *headers;
//  HttpHeader* headers = nullptr;
//  size_t      headerKeysCount = 0;
};

typedef std::function<void(Response response)>              ResponseHandler;
typedef std::function<void(uint8_t *data, size_t len)>      DataHandler;
typedef std::function<void(String data)>                    DataStringHandler;
typedef std::function<void(int error)>                      ErrorHandler;
typedef std::function<void(void)>                           DisconnectedHandler;


class AsyncHttpClient {
  public:
    AsyncHttpClient() {}
    ~AsyncHttpClient();
    
    AsyncHttpClient& GET(String url);
    void stop();
    inline void close()                                         { this->stop();               }
    inline AsyncHttpClient& onResponse(ResponseHandler handler) { responseHandler = handler;  return *this; }
    inline AsyncHttpClient& onData(DataHandler handler)         { dataHandler = handler;  dataStringHandler = NULL;    return *this; }
    inline AsyncHttpClient& onData(DataStringHandler handler)   { dataHandler = NULL;     dataStringHandler = handler;  return *this; }
    inline AsyncHttpClient& onError(ErrorHandler handler)       { errorHandler = handler;    return *this;  }
    inline AsyncHttpClient& onDisconnected(DisconnectedHandler handler) { disconnectedHandler = handler;  return *this; }

    // data หากมีการตัด chunked ส่งมา หากเป็น data ชุดแรกที่ไหลเข้ามา ใช่หรือไม่ (ไม่นับส่วน header)
    inline bool isFirstData()                                   { return _firstData; }

    bool   hasHeader(String name);
    inline int    headers()                { return response.headers.size(); } // จำนวน headers ที่ตอบกลับมา
    String header(String name);           // get request header value by name
    String header(size_t i);              // get request header value by number
    String headerName(size_t i);          // get request header name by number
    private:
    AsyncClient       *_client=NULL;
    AsyncClientSecure *_client_ssl=NULL;
    HttpState   state;
    Request     request;
    Response    response;
    bool _firstData;
    ResponseHandler     responseHandler = NULL;
    ErrorHandler        errorHandler = NULL;
    DataHandler         dataHandler = NULL;
    DataStringHandler   dataStringHandler = NULL;
    DisconnectedHandler disconnectedHandler = NULL;
    size_t bodyDownloadedSize;
    

    void startRequest();
    void sendRequest();
    void handleConnect();
    void handleDisconnect();
    void handleTimeout(int timeout);
    void handleError(int error);
    void handleData(uint8_t *data, size_t length);

    void processStatusLine(String line);
    void processHeaderLine(String line);
};

#endif //__BLYNKGO_ASYNC_HTTPCLIENT_H__
