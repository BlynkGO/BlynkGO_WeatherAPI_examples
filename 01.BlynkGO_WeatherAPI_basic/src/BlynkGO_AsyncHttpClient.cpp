#include "BlynkGO_AsyncHttpClient.h"

AsyncHttpClient::~AsyncHttpClient(){
  if(_client)       _client->stop();
  if(_client_ssl)   _client_ssl->stop();
  // ยังไม่ ล้าง mem ที่จอง ***
  if(response.headers.size()) {
    response.headers.clear();
  }
}

void AsyncHttpClient::processStatusLine(String line) {
//  Serial.println(line);
  response.httpCode = -1;
  if( line.startsWith("HTTP/")){
    response.httpCode = line.substring(line.indexOf(" ")+1).toInt();
  }
//  Serial.printf("[AsyncHTTP] Http Code : %d\n", response.httpCode );
}

void AsyncHttpClient::processHeaderLine(String headerLine) {
  String headerName  = headerLine.substring(0, headerLine.indexOf(':'));
  String headerValue = headerLine.substring(headerLine.indexOf(':') + 1); headerValue.trim();
  if(headerName.equalsIgnoreCase("Content-Length")) {
    response.contentLength = headerValue.toInt();
  }
  else if ( headerName.equalsIgnoreCase("Content-Type")) {
    response.contentType = headerValue;
  }
  
  HttpHeader new_header;
    new_header.key    = headerName;
    new_header.value  = headerValue;
    response.headers.push_back(new_header);
  

//  for(size_t i = 0; i < _headerKeysCount; i++) {
//    if(_currentHeaders[i].key.equalsIgnoreCase(headerName)) {
//      _currentHeaders[i].value = headerValue;
//      break;
//    }
//  }
//  if(_canReuse && headerName.equalsIgnoreCase("Connection")) {
//      if(headerValue.indexOf("close") >= 0 && headerValue.indexOf("keep-alive") < 0) {
//          _canReuse = false;
//      }
//  }


//  unsigned long split = line.indexOf(": ");
//  String headerName = line.substring(0, split);
//  String headerValue = line.substring(split + 2);
//  if (!strcasecmp(headerName.c_str(), "content-length"))
//    response.contentSize = headerValue.toInt();
//  if (!strcasecmp(headerName.c_str(), "content-type"))
//    response.contentType = headerValue;

//  int headersSize = sizeof(response.headers) / sizeof(String);
//  String *h = new String[headersSize + 1];
//  std::copy(response.headers, response.headers + headersSize, h);
//  delete[] response.headers;
//  response.headers = h;
//  response.headers[headersSize + 1] = line;
}


void AsyncHttpClient::handleConnect() {
  this->sendRequest();
}

void AsyncHttpClient::handleDisconnect() {
  state = ASYNC_STATE_IDLE;
  if(disconnectedHandler!=NULL) disconnectedHandler();

//  if( !request.is_ssl ) {
//    if( _client != NULL)      { delete _client; free(_client); _client = NULL; }
//  }else{
//    if( _client_ssl != NULL)  { delete _client; free(_client_ssl); _client = NULL; }    
//  }
}

void AsyncHttpClient::handleTimeout(int timeout) {
  state = ASYNC_STATE_IDLE;
  if (errorHandler != NULL)
    errorHandler(HTTP_ERROR_TIMED_OUT);
}

void AsyncHttpClient::handleError(int error) {
  state = ASYNC_STATE_IDLE;
  if (errorHandler != NULL)
    errorHandler(HTTP_ERROR_CONNECTION_FAILED);
}

void AsyncHttpClient::handleData(uint8_t *data, size_t length) {
  int index = 0;
  while (state == ASYNC_STATE_READING_STATUS_LINE || state == ASYNC_STATE_READING_HEADERS) {
    String line = "";
    while (index < length) {
      char c = (char) data[index];
      index++;
      if (c == '\n')
        break;
      else if (c != '\r')
        line += c;
    }

    if (state == ASYNC_STATE_READING_STATUS_LINE) {
      processStatusLine(line);
      state = ASYNC_STATE_READING_HEADERS;
    } else if (state == ASYNC_STATE_READING_HEADERS) {
      if (line.length() != 0) {
//        Serial.printf("[AsyncHTTP][Header] %s\n",line.c_str());
        processHeaderLine(line);
      }else {
        state = ASYNC_STATE_READING_BODY;
        if (responseHandler != NULL)
          responseHandler(response);
      }
    }
    if (index == length)
      break;
  }
  if (state == ASYNC_STATE_READING_BODY) {
    size_t size = length - index;
    if(size == 0) return;
    bodyDownloadedSize += size;
    if (dataHandler != NULL){
      dataHandler(data + index, size);
    }
    else if( dataStringHandler != NULL){
      char buf[size]; memcpy(buf, data+ index, size); buf[size]=0;
      dataStringHandler( buf );
    }
    if(_firstData==true) _firstData = false;
  }
}

bool AsyncHttpClient::hasHeader(String name){
  for(int i=0; i < response.headers.size(); i++){
    if( response.headers[i].key == name ) return true;
  }
  return false;
}

String AsyncHttpClient::header(String name){
  for(int i=0; i < response.headers.size(); i++){
    if( response.headers[i].key == name ) return response.headers[i].value;
  }
  return "";
}

String AsyncHttpClient::header(size_t i){
  if( i >= response.headers.size()) return "";
  return response.headers[i].value;
}
String AsyncHttpClient::headerName(size_t i){
  if( i >= response.headers.size()) return "";
  return response.headers[i].key;
}

void AsyncHttpClient::sendRequest() {
//  Serial.println("[AsyncHttp] send Request..");
  String s;
  if (request.method == ASYNC_HTTP_GET)         s = "GET";
  else if (request.method == ASYNC_HTTP_POST)   s = "POST";

  s += " " + String(request.path) + " HTTP/1.1\n";
  s += "Host: " + request.hostname + "\n";
  
  if (request.method == ASYNC_HTTP_POST){
    s += "Content-Type: " + request.contentType + "\n";
  }
  s += "Content-Size: " + String(request.dataSize) + "\n";
  s += "\n";

  state = ASYNC_STATE_READING_STATUS_LINE;
  response.headers.clear();
  _firstData = true;
  if (request.method == ASYNC_HTTP_GET) {
    if( !request.is_ssl ) {
      if(_client!=NULL)
        _client->write(s.c_str(), s.length());
    }else{
      if(_client_ssl != NULL)
        _client_ssl->write(s.c_str(), s.length());
    }
    
  }else if (request.method == ASYNC_HTTP_POST) {
    _client->add(s.c_str(), s.length());
    _client->add( (const char*) request.data, request.dataSize);
    _client->send();
  }
}

void AsyncHttpClient::startRequest(){
  if(!request.is_ssl){
    if( _client == NULL) {
      _client = (AsyncClient*) esp32_malloc(sizeof(AsyncClient));  // จอง บน PSRAM
      new (_client) AsyncClient();  // new instance จาก PSRAM
//      std::unique_ptr<AsyncClient>(new (_client)AsyncClient());
    }

    _client->onConnect([this](void *arg, AsyncClient *client)                         { this->handleConnect(); });
    _client->onDisconnect([this](void *arg, AsyncClient *client)                      { this->handleDisconnect(); });
    _client->onTimeout([this](void *arg, AsyncClient *client, int timeout)            { this->handleTimeout(timeout); });
    _client->onError([this](void *arg, AsyncClient *client, int error)                { this->handleError(error); });
    _client->onData([this](void *arg, AsyncClient *client, void *data, size_t length) { this->handleData((uint8_t *)data, length); });
    
    if (!_client->connected()) {
//      Serial.println("[AsyncHTTP] HTTP connecting...");
      if(request.hostname != "") {
        _client->connect( request.hostname.c_str(), request.port);  
      } else{
        _client->connect( request.address, request.port);  
      }
      return;
    }
  }else{ // ssl
    if( _client_ssl == NULL) {
      _client_ssl = (AsyncClientSecure*) esp32_malloc(sizeof(AsyncClientSecure));
      new (_client_ssl) AsyncClientSecure();
//      std::unique_ptr<AsyncClientSecure>(new (_client_ssl)AsyncClientSecure());
    }

    _client_ssl->onConnect([this](void *arg, AsyncClient *client)                         { this->handleConnect(); });
    _client_ssl->onDisconnect([this](void *arg, AsyncClient *client)                      { this->handleDisconnect(); });
    _client_ssl->onTimeout([this](void *arg, AsyncClient *client, int timeout)            { this->handleTimeout(timeout); });
    _client_ssl->onError([this](void *arg, AsyncClient *client, int error)                { this->handleError(error); });
    _client_ssl->onData([this](void *arg, AsyncClient *client, void *data, size_t length) { this->handleData((uint8_t *)data, length); });
    
    if (!_client_ssl->connected()) {
//      Serial.println("[AsyncHTTP] HTTPS connecting...");
      if(request.hostname != "") {
        _client_ssl->connect( request.hostname.c_str(), request.port);  
      } else{
        _client_ssl->connect( request.address, request.port);  
      }
      return;
    }
    
  }
  sendRequest();
}

AsyncHttpClient& AsyncHttpClient::GET(String url){
  if(!WiFi.isConnected()) return *this;
  if( url.startsWith("http://") ||  url.startsWith("HTTP://")) {
    int16_t idx1= url.indexOf("/",8);    
    String host_part = url.substring(7, idx1);
    String path      = url.substring(idx1);
//    Serial.println(host_part);
    if( host_part.indexOf(":") > 0){
      request.hostname = host_part.substring(0,host_part.indexOf(":"));
      request.port     = host_part.substring(host_part.indexOf(":")).toInt();
    }else{
      request.hostname = host_part;
      request.port     = 80;
    }

    request.path    = path;
    request.method  = ASYNC_HTTP_GET;
    request.is_ssl  = false;
    request.contentType = "";

    if(request.data != NULL && request.dataSize > 0){
      esp32_free(request.data);
      request.dataSize = 0;
    }

    this->startRequest();
  }else 
  if( url.startsWith("https://" )  ||  url.startsWith("HTTPS://")) {
    int16_t idx1= url.indexOf("/",9);    
    String host_part = url.substring(8, idx1);
    String path      = url.substring(idx1);
//    Serial.println(host_part);
    if( host_part.indexOf(":") > 0){
      request.hostname = host_part.substring(0,host_part.indexOf(":"));
      request.port     = host_part.substring(host_part.indexOf(":")).toInt();
    }else{
      request.hostname = host_part;
      request.port     = 443;
    }

    request.path    = path;
    request.method  = ASYNC_HTTP_GET;
    request.is_ssl  = true;
    request.contentType = "";

    if(request.data != NULL && request.dataSize > 0){
      esp32_free(request.data);
      request.dataSize = 0;
    }

    this->startRequest();
  }else{
    Serial.println("[AsyncHttpClient] error : bad URL");
  }

  return *this;
}

void AsyncHttpClient::stop(){
  if( !request.is_ssl ) {
    if(_client !=NULL ) _client->stop();
  }else{
    if(_client_ssl !=NULL ) _client_ssl->stop();    
  }
}
