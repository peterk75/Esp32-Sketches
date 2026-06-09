#include <LittleFS.h>
#include <WiFi.h>
#include <WiFiManager.h> 
#include <ESPAsyncWebServer.h> 
#include <time.h>

#define PINTIC 27
#define TICFACTOR 0.0000067     // factor between number of tics -> mSv Cesium-137,inhalation 

// variables shared between main code and interrupt code
hw_timer_t * timer = NULL;
volatile uint32_t updateTime = 0;       // time for next update
volatile uint16_t tic_cnt = 0;
time_t offset = 0;

// To calculate a running average over 10 sec, we keep tic counts in 250ms intervals and add all 40 tic_buf values
#define TICBUFSIZE 40                   // running average buffer size
volatile uint16_t tic_buf[TICBUFSIZE];  // buffer for 40 times 250ms tick values (to have a running average for 10 seconds)
volatile uint16_t tic_buf_cnt = 0;
volatile uint32_t tic_sum = 0;

#define SEC10BUFSIZE 50                 // history array for displaymode==true
volatile uint16_t sec10 = 0;            // every 10 seconds counter
volatile uint16_t sec10_buf[SEC10BUFSIZE];  // buffer to hold 10 sec history (40*10 = 400 seconds)
volatile bool sec10updated = false;     // set to true when sec10_buf is updated

// Create AsyncWebServer object on port 80
AsyncWebServer server(80);
AsyncEventSource events("/events");

const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 0;
const int   daylightOffset_sec = 3600;

// #########################################################################
// Interrupt routine called on each click from the geiger tube
//
void IRAM_ATTR TicISR() {
  tic_cnt++;
}

// #########################################################################
// Convert tics to mSv
//
float tics2mSv(uint32_t tics) {
  return float(tics) * TICFACTOR;
}


// #########################################################################
// Interrupt timer routine called every 250 ms
//
void IRAM_ATTR onTimer() {
  tic_buf[tic_buf_cnt++] = tic_cnt;
  tic_cnt=random(100);
  tic_sum+=tic_cnt;
  time_t tt = millis()+offset;
  String data = String("{\"value\":") + tic_cnt + ", \"timestamp\":" + tt + "}";
  String dose = String(tics2mSv(tic_sum));
  //Serial.println(String("Envet ticks:") + data.c_str());
  events.send(data.c_str(), "ticks", millis());
  events.send(dose.c_str(), "dose", millis());
  tic_cnt = 0;
  if (tic_buf_cnt>=TICBUFSIZE) {
    uint16_t tot = 0;
    for (int i=0; i<TICBUFSIZE; i++) tot += tic_buf[i];
    sec10_buf[sec10++] = tot;
    tic_buf_cnt = 0;    
    if (sec10>=SEC10BUFSIZE) sec10 = 0;
    sec10updated = true;
  }
}

// #########################################################################
// convert millirem value to a log percentage on analog and bar graph
//
int mrem2perc(float mrem, int maxperc) {
  if (mrem<=0) {
    return 0;
  } else {
    int v = (float(maxperc)/3)*(log10(mrem)+1);
    if (v>maxperc) v=maxperc;
    if (v<0) v=0;
    return v;
  }
}

void setup() {

  Serial.begin(9600);
  Serial.println("Starting ....");

  WiFi.mode(WIFI_STA);
  WiFiManager wm;
  wm.setConnectTimeout(120);      // optional timeout before portal starts (power cut)
  wm.setConfigPortalTimeout(60);  // optional portal active period (safety)
  wm.autoConnect("Geiger");       // open, 192.168.4.1
  //wm.autoConnect("Portal", "PortalPW"); // or with password
 
  // Init and get the time
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  time(&offset);
  offset=offset*1000;
  Serial.print("Epoch time (UTC): ");
  Serial.println(offset);
  

  // Initialize LittleFS
  if(!LittleFS.begin()){
    Serial.println("An Error has occurred while mounting LittleFS");
        return;
  }

  // Print ESP32 Local IP Address
  Serial.println(WiFi.localIP());

  // Route for root / web page
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(LittleFS, "/index.html");
  });

  events.onConnect([](AsyncEventSourceClient *client){
    if (client->lastId()) {
      Serial.printf("Client reconnected! Last message ID: %u\n", client->lastId());
    }
    client->send("connected", NULL, millis(), 10000);
    Serial.printf("connected");
  });
  server.addHandler(&events);

  // Start server
  server.begin();
  updateTime = millis(); // Next update time

  // attach interrupt routine to TIC interface from the geiger counter module
  pinMode(PINTIC, INPUT);
  attachInterrupt(PINTIC, TicISR, FALLING);

  // attach interrupt routine to internal timer, to fire every 250 ms
  // The Arduino timerBegin() function cannot run at 4Hz natively on the ESP32-C6 because the 16-bit hardware prescaler overflows.
  timer = timerBegin(1000000);
  if (timer == NULL) {
        Serial.println("Error with the start of the timer");
        return;
    }
  timerAttachInterrupt(timer, &onTimer);
  timerAlarm(timer, 250000, true, 0); // 250 ms - 250000 microsecs
  
  Serial.println("Geiger counter ready");
}

void loop() {
}