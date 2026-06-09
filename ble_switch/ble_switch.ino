#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLE2902.h>


#define LED_PIN 8
#define EXTLED_PIN 1
#define SERVO_PIN 10
#define BUTTON_PIN 3
// the debounce time in millisecond, increase this time if it still chatters
#define DEBOUNCE_TIME  50 

// Variables will change:
int lastSteadyState = HIGH;       // the previous steady state from the input pin
int lastFlickerableState = LOW;  // the previous flickerable state from the input pin
int currentState;                // the current reading from the input pin

// the following variables are unsigned longs because the time, measured in
// milliseconds, will quickly become a bigger number than can be stored in an int.
unsigned long lastDebounceTime = 0;  // the last time the output pin was toggled

BLECharacteristic *pCharacteristic;
bool deviceConnect = false;
int txValue = 0;


//the server and characteristic( what its going to do when it connects to the client)
#define SERVICE_UUID          "3b212f37-8502-46a0-9de4-d8ddd8ac74a0"  
#define CHARACTERISTIC_UUID_RX "bec5bc7e-7f89-4694-95d4-f3413557609a"
#define CHARACTERISTIC_UUID_TX   "b334b468-2732-4bb6-ba6e-819cd70ff73a"
// test of the client has been successfully connected
class MyServerCallBack: public BLEServerCallbacks{
  void onConnect(BLEServer* pServer){
    deviceConnect = true;
  };

  void OnDisconnect(BLEServer* pServer){
    deviceConnect = false;
  }
};

void switchOn()
{
    Serial.println("Light ON");
    digitalWrite(LED_PIN, LOW);
    digitalWrite(EXTLED_PIN, LOW);
    digitalWrite(SERVO_PIN, HIGH);
    txValue=1;
}

void switchOff()
{
    Serial.println("Light OFF");
    digitalWrite(LED_PIN, HIGH);
    digitalWrite(EXTLED_PIN, HIGH);
    digitalWrite(SERVO_PIN, LOW);
    txValue=0;
}

void switchToggle()
{
  if (txValue)
    switchOff();
  else
    switchOn();
}


// the start of receiving commands
class MyCallbacks : public BLECharacteristicCallbacks {
    void onRead(BLECharacteristic *pCharacteristic) {
        pCharacteristic->setValue(txValue);
    }

    void onWrite(BLECharacteristic *pCharacteristic) {
        String value = pCharacteristic->getValue();
        if (value.length() > 0) {
            Serial.print("Received: ");
            for (int i = 0; i < value.length(); i++){
              Serial.print((uint8_t)value[i]);
              Serial.print(" ");
            }
            Serial.println();

            uint8_t command = (uint8_t)value[0];
            Serial.print("Interpreted as: ");
            Serial.println(command, DEC); // The commands received and what they do

            if (command == 1){
              switchOn();
            }else if (command == 0) {
              switchOff();
            }
            //the end of the command line
            Serial.println();
            Serial.println("END");
        }
    }
};

/*
void restart()
{
  pService->stop();
  BLEDevice::deinit();
}*/


void setup() {
    delay(1000);
    Serial.begin(9600);
    pinMode(LED_PIN, OUTPUT);
    pinMode(EXTLED_PIN, OUTPUT);
    pinMode(SERVO_PIN, OUTPUT);
    pinMode(BUTTON_PIN, INPUT_PULLUP);

    switchOn(); // at start button is released so it calls toggle switch

    // Initialize BLE
    BLEDevice::init("ESP32_BLE_Light");  // 


    BLEServer *pServer = BLEDevice::createServer();
    pServer->setCallbacks(new MyServerCallBack());

    // Create the BLE Service using custom UUID
    BLEService *pService = pServer->createService(SERVICE_UUID);  
    
    // Create the BLE Characteristic using custom UUID
    pCharacteristic = pService->createCharacteristic(
        CHARACTERISTIC_UUID_TX, 
        BLECharacteristic::PROPERTY_READ
    );
    pCharacteristic->setCallbacks(new MyCallbacks());
    
    pCharacteristic->addDescriptor(new BLE2902());

    BLECharacteristic *pCharacteristic = pService->createCharacteristic(
        CHARACTERISTIC_UUID_RX,  // Custom Characteristic UUID
        BLECharacteristic::PROPERTY_WRITE
    );

        // Attach callback for write operations
    pCharacteristic->setCallbacks(new MyCallbacks());

    // Start the service
    pService->start();

    // Start advertising the service
    pServer->advertiseOnDisconnect(true);
    pServer->getAdvertising()-> start();

    Serial.println("BLE Ready. Connect and send '0' or '1' to control LED.");
    switchOff();
}

void loop() {
  // read the state of the switch/button:
  currentState = digitalRead(BUTTON_PIN);

  // check to see if you just pressed the button
  // (i.e. the input went from LOW to HIGH), and you've waited long enough
  // since the last press to ignore any noise:

  // If the switch/button changed, due to noise or pressing:
  if (currentState != lastFlickerableState) {
    // reset the debouncing timer
    lastDebounceTime = millis();
    // save the the last flickerable state
    lastFlickerableState = currentState;
  }

  if ((millis() - lastDebounceTime) > DEBOUNCE_TIME) {
    // whatever the reading is at, it's been there for longer than the debounce
    // delay, so take it as the actual current state:

    // if the button state has changed:
    if(lastSteadyState == HIGH && currentState == LOW)
      Serial.println("The button is pressed");
    else if(lastSteadyState == LOW && currentState == HIGH)
    {
     
      Serial.println("The button is released");
      switchToggle();
    }

    // save the the last steady state
    lastSteadyState = currentState;
  }
}
