// ESP32 + DHT22 -> verified HTTPS -> ThingSpeak channel + email alerts.
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <DHT.h>
#include <Preferences.h>
#include <time.h>
#include "config.h"
#include "certificate.h"
#include "logic.h"
DHT dht(4,DHT22);
Preferences prefs;
Alarm alarmState;
const uint32_t SAMPLE_MS=20000, RECONNECT_MS=15000;
const time_t COOLDOWN_SECONDS=1860; // 31 min, conservative versus cloud quota
uint32_t lastSample=0,lastReconnect=0;
time_t lastAlert=0,lastAlertTry=0;
bool clockReady() { return time(nullptr)>1700000000; }
String encode(const String& s) {
 String r; const char* hex="0123456789ABCDEF";
 for(size_t i=0;i<s.length();++i) { unsigned char c=s[i];
   if(isalnum(c)||c=='-'||c=='_'||c=='.'||c=='~') r+=char(c);
   else { r+='%'; r+=hex[c>>4]; r+=hex[c&15]; }
 } return r;
}
bool upload(float t,float h) {
 WiFiClientSecure tls; tls.setCACert(ROOT_CA); tls.setHandshakeTimeout(10);
 HTTPClient http; http.setTimeout(10000);
 if(!http.begin(tls,"https://api.thingspeak.com/update")) return false;
 http.addHeader("Content-Type","application/x-www-form-urlencoded");
 int code=http.POST("api_key="+encode(WRITE_KEY)+"&field1="+String(t,2)+"&field2="+String(h,2));
 String body=http.getString(); http.end(); body.trim();
 bool ok=code==200&&body.toInt()>0;
 Serial.printf("Upload HTTP=%d, accepted=%s",code,ok?"yes":"no");
 if(ok) Serial.printf(", entry=%ld",body.toInt());
 Serial.println(); return ok;
}
void alert(float t,float h) {
 time_t now=time(nullptr);
 if(!clockReady()||(lastAlert&&now-lastAlert<COOLDOWN_SECONDS)||
    (lastAlertTry&&now-lastAlertTry<COOLDOWN_SECONDS)) return;
 // Persist attempt before HTTP: reboots and failed responses cannot flood inbox.
 lastAlertTry=now; prefs.putULong64("attempt",uint64_t(now));
 WiFiClientSecure tls; tls.setCACert(ROOT_CA); tls.setHandshakeTimeout(10);
 HTTPClient http; http.setTimeout(10000);
 if(!http.begin(tls,"https://api.thingspeak.com/alerts/send")) return;
 http.addHeader("Content-Type","application/x-www-form-urlencoded");
 http.addHeader("ThingSpeak-Alerts-API-Key",ALERT_KEY);
 String body="Weather station threshold exceeded. Temperature="+String(t,2)+
 " C; humidity="+String(h,2)+" %. Limits: >35 C or >80 %. Channel "+String(CHANNEL_ID)+".";
 int code=http.POST("subject="+encode("Weather station threshold alert")+"&body="+encode(body));
 http.end(); Serial.printf("Alert HTTP=%d (202 means queued, not inbox delivery)\n",code);
 if(code==202) { lastAlert=now; prefs.putULong64("accepted",uint64_t(now)); }
}
void setup() {
 Serial.begin(115200); dht.begin();
 prefs.begin("weather",false);
 lastAlert=time_t(prefs.getULong64("accepted",0));
 lastAlertTry=time_t(prefs.getULong64("attempt",0));
 WiFi.mode(WIFI_STA); WiFi.setAutoReconnect(true); WiFi.begin(WIFI_SSID,WIFI_PASSWORD);
 configTime(0,0,"pool.ntp.org","time.nist.gov");
 lastSample=millis(); lastReconnect=millis();
 Serial.println("Weather station ready. First sample after 20 seconds.");
}
void loop() {
 uint32_t now=millis();
 if(WiFi.status()!=WL_CONNECTED) {
   if(elapsed(now,lastReconnect,RECONNECT_MS)) {
     lastReconnect=now; WiFi.reconnect(); Serial.println("Reconnecting Wi-Fi...");
   }
 }
 if(!elapsed(now,lastSample,SAMPLE_MS)) { delay(10); return; }
 lastSample=now;
 float h=dht.readHumidity(),t=dht.readTemperature();
 if(!validReading(t,h)) { Serial.println("Invalid DHT22 reading; skipped."); return; }
 Serial.printf("T=%.2f C, RH=%.2f %%\n",t,h);
 bool active=alarmState.update(t,h);
 if(WiFi.status()!=WL_CONNECTED) { Serial.println("Offline; sample not buffered."); return; }
 if(!clockReady()) { Serial.println("Waiting for NTP before TLS."); return; }
 upload(t,h);
 if(active) alert(t,h);
 // Start next interval after network work to preserve channel write spacing.
 lastSample=millis();
}
