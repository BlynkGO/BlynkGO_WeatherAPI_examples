/**********************************************************
 * สมัคร https://www.weatherapi.com/ แล้ว นำ API KEY มากำหนด
 * request ได้ 1 ล้านครั้งต่อวัน (แบบฟรี)
 * *******************************************************/

#include <BlynkGOv2.h>
#include <ArduinoJson.h>  // version 6.x
#include <SPIFFS.h>
#include "src/BlynkGO_AsyncHttpClient.h"

#define WEATHERAPI_KEY    "-------------------------------"
#define GPS_LAT           13.7516435
#define GPS_LONG          100.4927041
#define WEATHER_INTERVAL  10000        // ทุกๆ 10 วินาที

IMG_DECLARE(ico_weather);

GWiFiManager  wifi_manager;
GLabel lb_clock;
GIcon icon_weather(ico_weather, "พยากรณ์อากาศ");

GRect app_weather;
  GImage img_weather          (app_weather);
  GLabel lb_title_weather     (app_weather);
  GLabel lb_gps               (app_weather);
  GLabel lb_last_updated      (app_weather);
  GContainer cont_us_aqi      (app_weather);
    GLabel lb_aqi("AQI : ",cont_us_aqi);
    GRect rect_us_aqi[6];     // สี่เหลี่ยมช่องสีของระดับ US AQI
      String us_aqi_str[6] = {"Good", "Moderate", "Unhealthy for sensitive group", "Unhealthy", "Very Unhealthy", "Hazardous" }; 
      color_t us_aqi_color[6] = { TFT_COLOR_HEX(0x01cc00), TFT_COLOR_HEX(0xffc000), TFT_COLOR_HEX(0xff6600),
                                  TFT_COLOR_HEX(0xff0000), TFT_COLOR_HEX(0x9b31ff), TFT_COLOR_HEX(0x970232),};
  GCircle cir_us_aqi_indic    (app_weather);  // วงกลมจุด สำหรับเป็น indic (ตัวชี้) AQI ปัจจุบัน

  GLabel lb_weather_text      (app_weather);

void setup() {
  Serial.begin(115200); Serial.println();
  BlynkGO.begin();

  wifi_manager.align(ALIGN_TOP_RIGHT,-10,5);
  lb_clock.align(wifi_manager, ALIGN_LEFT,-10);

  icon_weather.onClicked([](GWidget*widget){
    Serial.println("[Weather] App clicked");
    app_weather.hidden(false);
    static GTimer timer;
    timer.delay(5000,[](){  app_weather.hidden(true);  });
  });

  //---------------------------------------------------
  // ออกแบบ app_weather ตามต้องการ
  app_weather.hidden(true);
  app_weather.color(TFT_COLOR_HSV(240,100,20));  // สีน้ำเงินโทนเข้ม
  app_weather.onClicked([](GWidget*widget){
    app_weather.hidden(true);
  });
    lb_title_weather = "พยากรณ์อากาศ";
    lb_title_weather.align(ALIGN_TOP_LEFT,15,10);
    lb_title_weather.font(prasanmit_40);
    
    lb_gps.align(ALIGN_TOP,0,70);
    lb_gps = StringX::printf("GPS : %f,%f", GPS_LAT, GPS_LONG);

    lb_last_updated.font(prasanmit_20, TFT_SILVER);
    lb_last_updated.align(ALIGN_BOTTOM_RIGHT,-10,3);

    img_weather.hidden(true);
    img_weather.align(lb_title_weather, ALIGN_BOTTOM_LEFT);

    cont_us_aqi.opa(0);
    cont_us_aqi.layout(LAYOUT_ROW_M);
    cont_us_aqi.padding(0,0,0,0,2);
    lb_aqi.font(prasanmit_25, TFT_WHITE);
    for(int i=0;i<6;i++){
      rect_us_aqi[i].size(25,8);
      rect_us_aqi[i].color(us_aqi_color[i]);
      rect_us_aqi[i].parent(cont_us_aqi);
    }
    cont_us_aqi.align(ALIGN_BOTTOM_LEFT, 10,5);
    
    cir_us_aqi_indic.hidden(true);
    cir_us_aqi_indic.size(5,5);
    cir_us_aqi_indic.color(TFT_WHITE);
    cir_us_aqi_indic.align(rect_us_aqi[0] ,ALIGN_TOP,5,5);

    lb_weather_text.font(prasanmit_40);
}


void loop() {
  BlynkGO.update();
}

WIFI_CONNECTED(){
  static GTimer timer_weatherapi;
  timer_weatherapi.setInterval(WEATHER_INTERVAL,[](){ 
    Serial.println("[HTTP] GET WeatherAPI ...");
    static AsyncHttpClient  http;
    http.GET(StringX::printf("http://api.weatherapi.com/v1/current.json?key=%s&q=%f,%f&aqi=yes",
                                  WEATHERAPI_KEY, GPS_LAT, GPS_LONG ))
        .onData([](String data) { // เมื่อมีการส่ง ข้อมูล กลับมา
//            Serial.println(data);
            String json_str = data.substring(data.indexOf("{"),data.lastIndexOf("}")+1);
            Serial.println(json_str);
            StaticJsonDocument<2048> doc;
            if(!deserializeJson(doc, json_str)){
              float   location_lat      = doc["location"]["lat"].as<float>(); // 13.75
              float   location_lon      = doc["location"]["lon"].as<float>(); // 100.49
              String  last_updated      = doc["current"]["last_updated"].as<String>();
              float   temp_c            = doc["current"]["temp_c"].as<float>();
              float   feelslike_c       = doc["current"]["feelslike_c"].as<float>();
              uint8_t humidity          = doc["current"]["humidity"].as<uint8_t>();
              String  weather_icon_url  = doc["current"]["condition"]["icon"].as<String>();
              String  weather_text      = doc["current"]["condition"]["text"].as<String>();              
              float   pm2_5             = doc["current"]["air_quality"]["pm2_5"].as<float>();
              uint8_t aqi_us_epa_index  = doc["current"]["air_quality"]["us-epa-index"].as<uint8_t>();
              float   wind_kph          = doc["current"]["wind_kph"].as<float>();
              uint16_t wind_degree      = doc["current"]["wind_degree"].as<uint16_t>();
              uint16_t pressure_mb      = doc["current"]["pressure_mb"].as<uint16_t>();
              float   precip_mm         = doc["current"]["precip_mm"].as<float>();
              uint8_t vis_km            = doc["current"]["vis_km"].as<uint8_t>();
              uint8_t uv                = doc["current"]["uv"].as<uint8_t>();
              float   gust_kph          = doc["current"]["gust_kph"].as<float>();

              // ทำการส่ง http ไปดึงรูป icon ของ weather ปัจจุบัน
              // มาแสดงเป็นรูปบน จอ
              weather_icon_url = "http:"+weather_icon_url;
              http_get_weather_icon(weather_icon_url);

              //-----------------------------------------------------------------
              // คำค่า weather ด้านบนที่ดึงมาได้จาก WeatherAPI ค่าต่างๆ นำไปใส่วิตเจ็ตที่ต้องการ

              icon_weather.text( StringX::printf("อุณฯ %.1f C", temp_c ) );
              lb_last_updated = String("updated : ") + last_updated;

              // เปลี่ยน ค่า ระดับ US AQI ไปเป็นตำแหน่งกราฟิก บอกระดับสี US AQI
              cir_us_aqi_indic.hidden(false);
              cir_us_aqi_indic.align(rect_us_aqi[aqi_us_epa_index-1] ,ALIGN_TOP,0,-3);

              lb_weather_text = weather_text;

            }
          }); // http.onData([](String data) ...
  }, true);   // timer_weatherapi.setInterval(..) ...
}

NTP_SYNCED(){
  static GTimer timer_clock;
  timer_clock.setInterval(1000,[](){
    lb_clock = StringX::printf("%02d:%02d:%02d", hour(), minute(), second());
  }); 
}

// ฟังกชั่น สำหรับดึงรูปจาก url ที่กำหนด มาวางที่ SPIFFS
// แล้วทำการ กำหนด ไฟล์รูปจาก SPIFFS ให้ วิตเจ็ตที่ต้องการแสดงรูปต่อไป
void http_get_weather_icon(String weather_icon_url){
  static AsyncHttpClient  http2;
  Serial.printf("[HTTP2] GET weather icon file : %s\n", weather_icon_url.c_str());
  http2 .GET( weather_icon_url )
        .onResponse([http2](Response response){
            Serial.printf("[HTTP2][Response] Http Code      : %d\n", response.httpCode);
            Serial.printf("[HTTP2][Response] Content Length : %d\n", response.contentLength);
            Serial.printf("[HTTP2][Response] Content Type   : %s\n", response.contentType.c_str());
//            // หากจะดู header ของ response ที่ส่งกลับมา
//            for(int i=0; i< http2.headers(); i++){
//              Serial.printf("[HTTP2][Header] %s : %s\n", 
//                  http2.headerName(i).c_str(),
//                  http2.header(i).c_str());
//            }
         }) // http2.onResponse(Response response) ...
        .onData([http2](uint8_t *data, size_t len){
            static File file;
            static int  file_size, file_index;
            
            if( http2.isFirstData() ) {
              Serial.println("[HTTP2] first Data!!");
              int index = 0;
              String line = "";
              char buf[len]; memset( buf, 0, len+1);
              int buf_i = 0;                          
              enum { DATA_STATE_FIRSTLINE, DATA_STATE_PAYLOAD };
              uint8_t data_state = DATA_STATE_FIRSTLINE;
              while( index < len ){
                char c = (char) data[index++];

                switch(data_state){
                  case DATA_STATE_FIRSTLINE:
                    if(c != '\n'){
                      line += c;
                    }else{
                      file_size  = (int)strtol(line.c_str(), NULL, 16);
                      file_index = 0;
                      file       = SPIFFS.open("/ico_weather.png", FILE_WRITE);                                  
//                      Serial.println(line);
                      Serial.printf("[HTTP2] file size : %d bytes\n",file_size);
                      Serial.println("--------------------");
                      data_state = DATA_STATE_PAYLOAD;                      
                    }
                    break;
                  case DATA_STATE_PAYLOAD:
                      file.write(c);
                      file_index++;
                    break;
                }
              }
              Serial.printf("[File] index : %d\n",file_index);
            }else{
              if(file_index < file_size) {
                if( file_index + len <= file_size ){
                  file.write(data,len);
                  file_index += len;
                }else{
                  file.write(data, file_size - file_index );
                  file_index += (file_size - file_index);
                }

                Serial.printf("[File] index : %d\n",file_index);                            
                if(file_index >= file_size){
                  if(file){
                    file.close();
                    Serial.println("[File] saved");
                    // รูป icon weather สถานะปัจจุบัน บันทึกเรียบร้อย 
                    // นำไปกำหนดให้ วิตเจ็ตที่ต้องการแสดงภาพต่อไป

                    File file_icon = SPIFFS.open("/ico_weather.png", FILE_READ);  
                    int file_size2 = file_icon.size();
                    file_icon.close();
                    Serial.printf("[File] SPIFFS file size : %d\n", file_size);
                    if( file_size2 == file_size ) {
                      icon_weather.setImage("SPIFFS://ico_weather.png"); // กำหนดรูปให้วิตเจ็ต GIcon
                      img_weather.hidden(false);
                      img_weather.setImage("SPIFFS://ico_weather.png");  // กำหนดรูปให้วิตเจ็ต GImage
                    }
                  }
                }
              }
            }
         }); // http2.onData([](uint8_t *data, size_t len)...
}
