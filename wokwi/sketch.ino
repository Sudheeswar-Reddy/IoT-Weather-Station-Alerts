// Wokwi simulation edition. Uses simulated DHT22, not physical hardware.
// ESP32 + DHT22 -> verified HTTPS -> ThingSpeak channel + email alerts.
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <DHTesp.h>
#include <Preferences.h>
#include <time.h>
// Keys are entered through the serial console at runtime; never saved here.
String WRITE_KEY, ALERT_KEY;
constexpr unsigned long CHANNEL_ID=3537247;
bool configured=false;


// DigiCert Global Root G2, extracted from Mozilla CA bundle via certifi.
static const char ROOT_CA[] = R"CERT(-----BEGIN CERTIFICATE-----
MIIDjjCCAnagAwIBAgIQAzrx5qcRqaC7KGSxHQn65TANBgkqhkiG9w0BAQsFADBh
MQswCQYDVQQGEwJVUzEVMBMGA1UEChMMRGlnaUNlcnQgSW5jMRkwFwYDVQQLExB3
d3cuZGlnaWNlcnQuY29tMSAwHgYDVQQDExdEaWdpQ2VydCBHbG9iYWwgUm9vdCBH
MjAeFw0xMzA4MDExMjAwMDBaFw0zODAxMTUxMjAwMDBaMGExCzAJBgNVBAYTAlVT
MRUwEwYDVQQKEwxEaWdpQ2VydCBJbmMxGTAXBgNVBAsTEHd3dy5kaWdpY2VydC5j
b20xIDAeBgNVBAMTF0RpZ2lDZXJ0IEdsb2JhbCBSb290IEcyMIIBIjANBgkqhkiG
9w0BAQEFAAOCAQ8AMIIBCgKCAQEAuzfNNNx7a8myaJCtSnX/RrohCgiN9RlUyfuI
2/Ou8jqJkTx65qsGGmvPrC3oXgkkRLpimn7Wo6h+4FR1IAWsULecYxpsMNzaHxmx
1x7e/dfgy5SDN67sH0NO3Xss0r0upS/kqbitOtSZpLYl6ZtrAGCSYP9PIUkY92eQ
q2EGnI/yuum06ZIya7XzV+hdG82MHauVBJVJ8zUtluNJbd134/tJS7SsVQepj5Wz
tCO7TG1F8PapspUwtP1MVYwnSlcUfIKdzXOS0xZKBgyMUNGPHgm+F6HmIcr9g+UQ
vIOlCsRnKPZzFBQ9RnbDhxSJITRNrw9FDKZJobq7nMWxM4MphQIDAQABo0IwQDAP
BgNVHRMBAf8EBTADAQH/MA4GA1UdDwEB/wQEAwIBhjAdBgNVHQ4EFgQUTiJUIBiV
5uNu5g/6+rkS7QYXjzkwDQYJKoZIhvcNAQELBQADggEBAGBnKJRvDkhj6zHd6mcY
1Yl9PMWLSn/pvtsrF9+wX3N3KjITOYFnQoQj8kVnNeyIv/iPsGEMNKSuIEyExtv4
NeF22d+mQrvHRAiGfzZ0JFrabA0UWTW98kndth/Jsw1HKj2ZL7tcu7XUIOGZX1NG
Fdtom/DzMNU+MeKNhJ7jitralj41E6Vf8PlwUHBHQRFXGU7Aj64GxJUTFy8bJZ91
8rGOmaFvE7FBcf6IKshPECBV1/MUReXgRPTqh5Uykw7+U0b6LJ3/iyK5S9kJRaTe
pLiaWN0bfVKfjllDiIGknibVb63dDcY3fe0Dkhvld1927jyNxF1WW6LZZm6zNTfl
MrY=
-----END CERTIFICATE-----)CERT";


#include <math.h>
#include <stdint.h>
struct Alarm {
  bool hot=false, humid=false;
  bool update(float t, float h) {
    if (!isfinite(t)||!isfinite(h)||t < -40||t > 80||h < 0||h > 100) return false;
    if (t > 35) hot=true; else if(t <= 33) hot=false;
    if (h > 80) humid=true; else if(h <= 75) humid=false;
    return hot || humid;
  }
};
inline bool validReading(float t,float h) {
 return isfinite(t)&&isfinite(h)&&t>=-40&&t<=80&&h>=0&&h<=100;
}
inline bool elapsed(uint32_t now,uint32_t then,uint32_t interval) {
 return uint32_t(now-then)>=interval;
}

DHTesp dht;
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
 int code=http.POST("api_key="+encode(WRITE_KEY)+"&field1="+String(t,2)+"&field2="+String(h,2)+"&status="+encode("WOKWI simulated DHT22 - not physical measurements"));
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
 String body="WOKWI SIMULATION: threshold exceeded. Temperature="+String(t,2)+
 " C; humidity="+String(h,2)+" %. Limits: >35 C or >80 %. Channel "+String(CHANNEL_ID)+".";
 int code=http.POST("subject="+encode("Wokwi weather station threshold alert")+"&body="+encode(body));
 http.end(); Serial.printf("Alert HTTP=%d (202 means queued, not inbox delivery)\n",code);
 if(code==202) { lastAlert=now; prefs.putULong64("accepted",uint64_t(now)); }
}
void setup() {
 Serial.begin(115200); Serial.setTimeout(100); dht.setup(15,DHTesp::DHT22);
 Alarm test;
 bool pass=validReading(28,60)&&!validReading(NAN,60)&&!validReading(28,101)&&
 !test.update(35,80)&&test.update(36,60)&&test.update(34,60)&&!test.update(33,60)&&
 test.update(25,81)&&test.update(25,77)&&!test.update(25,75)&&
 !elapsed(10,0xfffffff0u,30)&&elapsed(20,0xfffffff0u,30);
 Serial.println(pass ? "SELF-TEST PASS: validation, thresholds, hysteresis, rollover" : "SELF-TEST FAILED");
 if(!pass) while(true) delay(1000);

 prefs.begin("weather",false);
 lastAlert=time_t(prefs.getULong64("accepted",0));
 lastAlertTry=time_t(prefs.getULong64("attempt",0));
 WiFi.mode(WIFI_STA); WiFi.setAutoReconnect(true); WiFi.begin("Wokwi-GUEST","",6);
 configTime(0,0,"pool.ntp.org","time.nist.gov");
 lastSample=millis(); lastReconnect=millis();
 Serial.println("WOKWI ESP32 + DHT22 -> ThingSpeak 3537247");
 Serial.println("Enter CONFIG <channel_write_key> <alerts_key> in serial input, then Enter.");
 Serial.println("Keys stay in RAM and are not printed or stored in the sketch.");
}
void loop() {
 if(Serial.available()) {
   String cmd=Serial.readStringUntil('\n'); cmd.trim();
   if(cmd.startsWith("CONFIG ")) {
     int split=cmd.indexOf(' ',7);
     if(split>7) { WRITE_KEY=cmd.substring(7,split); ALERT_KEY=cmd.substring(split+1); ALERT_KEY.trim();
       configured=WRITE_KEY.length()==16&&ALERT_KEY.startsWith("TAK");
       Serial.println(configured ? "CONFIG accepted; keys held in RAM." : "CONFIG invalid; expected write and alert keys.");
     }
   }
 }
 uint32_t now=millis();
 if(WiFi.status()!=WL_CONNECTED) {
   if(elapsed(now,lastReconnect,RECONNECT_MS)) {
     lastReconnect=now; WiFi.reconnect(); Serial.println("Reconnecting Wi-Fi...");
   }
 }
 if(!elapsed(now,lastSample,SAMPLE_MS)) { delay(10); return; }
 lastSample=now;
 TempAndHumidity sample=dht.getTempAndHumidity(); float h=sample.humidity,t=sample.temperature;
 if(!validReading(t,h)) { Serial.printf("Invalid DHT22 reading: %s; skipped.\n",dht.getStatusString()); return; }
 Serial.printf("T=%.2f C, RH=%.2f %%\n",t,h);
 bool active=alarmState.update(t,h);
 Serial.printf("Alarm=%s; WiFi=%s; NTP=%s\n",active?"active":"clear",WiFi.status()==WL_CONNECTED?"connected":"offline",clockReady()?"ready":"waiting");
 if(!configured) { Serial.println("Cloud paused: enter CONFIG command."); return; }
 if(WiFi.status()!=WL_CONNECTED) { Serial.println("Offline; sample not buffered."); return; }
 if(!clockReady()) { Serial.println("Waiting for NTP before TLS."); return; }
 upload(t,h);
 if(active) alert(t,h);
 // Start next interval after network work to preserve channel write spacing.
 lastSample=millis();
}
