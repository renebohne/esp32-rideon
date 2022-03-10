#include "BLEDevice.h"

// Bit positions in the 74HCT595 shift register output
#define MOTOR4_A 0
#define MOTOR2_A 1
#define MOTOR1_A 2
#define MOTOR1_B 3
#define MOTOR2_B 4
#define MOTOR3_A 5
#define MOTOR4_B 6
#define MOTOR3_B 7

// Arduino pin names for interface to 74HC595 latch via D1 R32 esp32
#define MOTORLATCH 19
#define MOTORCLK 17
#define MOTORENABLE 14
#define MOTORDATA 12

//PWM Pins for controlling motor speed
#define PWM0A 27
#define PWM_MOTOR3 PWM0A
#define PWM0B 16
#define PWM_MOTOR4 PWM0B
#define PWM2A 23
#define PWM_MOTOR1 PWM2A
#define PWM2B 25
#define PWM_MOTOR2 PWM2B

#define PWM_1B 5
#define PWM_SERVO1 PWM_1B

#define PWM_1A 13
#define PWM_SERVO2 PWM_1A

#define SERVO1_MIN_PWM 1851
#define SERVO1_MAX_PWM 8251

#define SERVO2_MIN_PWM 1851
#define SERVO2_MAX_PWM 8251

#define FORWARD 1
#define BACKWARD 2
#define BRAKE 3
#define RELEASE 4

static uint8_t latch_state;

uint16_t x_max = 1803;
uint16_t x_min = 173;
uint16_t x_center = 991;
uint16_t x_val = x_center;

uint16_t y_max = 1803;
uint16_t y_min = 173;
uint16_t y_center = 991;
uint16_t y_val = 0;

// The remote service we wish to connect to.
static BLEUUID serviceUUID("4fafc201-1fb5-459e-8fcc-c5c9c331914b");
// The characteristic of the remote service we are interested in.
static BLEUUID    charUUID("beb5483e-36e1-4688-b7f5-ea07361b26a8");
static boolean doConnect = false;
static boolean connected = false;
static boolean doScan = false;
static BLERemoteCharacteristic* pRemoteCharacteristic;
static BLEAdvertisedDevice* myDevice;



void enable(void) {
  // setup the latch
  pinMode(MOTORLATCH, OUTPUT);
  pinMode(MOTORENABLE, OUTPUT);
  pinMode(MOTORDATA, OUTPUT);
  pinMode(MOTORCLK, OUTPUT);

  latch_state = 0;
  latch_tx();  // "reset"
  // enable the chip outputs!
  digitalWrite(MOTORENABLE, LOW);
}


void latch_tx(void) {
  uint8_t i;
  digitalWrite(MOTORLATCH, LOW);
  digitalWrite(MOTORDATA, LOW);

  for (i = 0; i < 8; i++)
  {
    digitalWrite(MOTORCLK, LOW);
    digitalWrite(MOTORDATA, latch_state & _BV(7 - i));
    digitalWrite(MOTORCLK, HIGH);
  }
  digitalWrite(MOTORLATCH, HIGH);
}

void run(uint8_t motornum, uint8_t cmd) {
  uint8_t a, b;
  switch (motornum) {
    case 1:
      a = MOTOR1_A;
      b = MOTOR1_B;
      break;
    case 2:
      a = MOTOR2_A;
      b = MOTOR2_B;
      break;
    case 3:
      a = MOTOR3_A;
      b = MOTOR3_B;
      break;
    case 4:
      a = MOTOR4_A;
      b = MOTOR4_B;
      break;
    default:
      return;
  }

  switch (cmd) {
    case FORWARD:
      latch_state |= _BV(a);
      latch_state &= ~_BV(b);
      latch_tx();
      break;
    case BACKWARD:
      latch_state &= ~_BV(a);
      latch_state |= _BV(b);
      latch_tx();
      break;
    case RELEASE:
      latch_state &= ~_BV(a);     // A and B both low
      latch_state &= ~_BV(b);
      latch_tx();
      break;
  }
}




static void notifyCallback(
  BLERemoteCharacteristic* pBLERemoteCharacteristic,
  uint8_t* pData,
  size_t length,
  bool isNotify) {
  //Serial.print("Notify callback for characteristic ");
  //Serial.print(pBLERemoteCharacteristic->getUUID().toString().c_str());
  //Serial.print(" of data length ");
  //Serial.println(length);
  //Serial.print("data: ");
  /*
    for(int i=0;i<2;i++)
    {
    Serial.print(pData[i],HEX);
    Serial.print(" ");
    }*/

  memcpy(&x_val, pData, 2);
  memcpy(&y_val, pData + 2, 2);

  if (x_val > x_max)
  {
    x_max = x_val;
    x_center = (x_min + x_max) / 2;
  }
  if (x_val < x_min)
  {
    x_min = x_val;
    x_center = (x_min + x_max) / 2;
  }

  if (y_val > y_max)
  {
    y_max = y_val;
    y_center = (y_min + y_max) / 2;
  }
  if (y_val < y_min)
  {
    y_min = y_val;
    y_center = (y_min + y_max) / 2;
  }



}

class MyClientCallback : public BLEClientCallbacks {
    void onConnect(BLEClient* pclient) {
    }

    void onDisconnect(BLEClient* pclient) {
      connected = false;
      Serial.println("onDisconnect");
    }
};

bool connectToServer() {
  Serial.print("Forming a connection to ");
  Serial.println(myDevice->getAddress().toString().c_str());

  BLEClient*  pClient  = BLEDevice::createClient();
  Serial.println(" - Created client");

  pClient->setClientCallbacks(new MyClientCallback());

  // Connect to the remove BLE Server.
  pClient->connect(myDevice);  // if you pass BLEAdvertisedDevice instead of address, it will be recognized type of peer device address (public or private)
  Serial.println(" - Connected to server");
  pClient->setMTU(517); //set client to request maximum MTU from server (default is 23 otherwise)

  // Obtain a reference to the service we are after in the remote BLE server.
  BLERemoteService* pRemoteService = pClient->getService(serviceUUID);
  if (pRemoteService == nullptr) {
    Serial.print("Failed to find our service UUID: ");
    Serial.println(serviceUUID.toString().c_str());
    pClient->disconnect();
    return false;
  }
  Serial.println(" - Found our service");


  // Obtain a reference to the characteristic in the service of the remote BLE server.
  pRemoteCharacteristic = pRemoteService->getCharacteristic(charUUID);
  if (pRemoteCharacteristic == nullptr) {
    Serial.print("Failed to find our characteristic UUID: ");
    Serial.println(charUUID.toString().c_str());
    pClient->disconnect();
    return false;
  }
  Serial.println(" - Found our characteristic");

  // Read the value of the characteristic.
  if (pRemoteCharacteristic->canRead()) {
    std::string value = pRemoteCharacteristic->readValue();
    Serial.print("The characteristic value was: ");
    Serial.println(value.c_str());
  }

  if (pRemoteCharacteristic->canNotify())
    pRemoteCharacteristic->registerForNotify(notifyCallback);

  connected = true;
  return true;
}
/**
   Scan for BLE servers and find the first one that advertises the service we are looking for.
*/
class MyAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks {
    /**
        Called for each advertising BLE server.
    */
    void onResult(BLEAdvertisedDevice advertisedDevice) {
      Serial.print("BLE Advertised Device found: ");
      Serial.println(advertisedDevice.toString().c_str());

      // We have found a device, let us now see if it contains the service we are looking for.
      //if (advertisedDevice.haveServiceUUID() && advertisedDevice.isAdvertisingService(serviceUUID)) {
      if (advertisedDevice.getAddress() == BLEAddress("84:0d:8e:2b:57:ea"))
      {


        BLEDevice::getScan()->stop();
        myDevice = new BLEAdvertisedDevice(advertisedDevice);
        doConnect = true;
        doScan = true;

      } // Found our server
    } // onResult
}; // MyAdvertisedDeviceCallbacks



void setup() {
  Serial.begin(115200);
  enable();



  // 18 kHz PWM, 8-bit resolution
  ledcSetup(1, 18000, 8);
  ledcSetup(2, 18000, 8);
  ledcSetup(3, 18000, 8);
  ledcSetup(4, 18000, 8);
  //servos at 50hz and 16-bit resolution
  ledcSetup(8, 50, 16);
  //ledcSetup(6, 50, 16);

    // assign MOTOR PWM pins to channels
  ledcAttachPin(PWM_MOTOR1, 1);
  ledcAttachPin(PWM_MOTOR2, 2);
  ledcAttachPin(PWM_MOTOR3, 3);
  ledcAttachPin(PWM_MOTOR4, 4);
  ledcAttachPin(PWM_SERVO1, 8);
  //ledcAttachPin(PWM_SERVO2, 6);

  BLEDevice::init("");
  // Retrieve a Scanner and set the callback we want to use to be informed when we
  // have detected a new device.  Specify that we want active scanning and start the
  // scan to run for 5 seconds.
  BLEScan* pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setInterval(1349);
  pBLEScan->setWindow(449);
  pBLEScan->setActiveScan(true);
  pBLEScan->start(5, false);

}

int lastDirection = RELEASE;

void stop()
{
  run(1, RELEASE);
  run(2, RELEASE);
  run(3, RELEASE);
  run(4, RELEASE);
  ledcWrite(1, 0);
  ledcWrite(2, 0);
  ledcWrite(3, 0);
  ledcWrite(4, 0);
  //ledcWrite(8, (SERVO1_MIN_PWM+SERVO1_MAX_PWM)/2);
  //ledcWrite(6, (SERVO2_MIN_PWM+SERVO2_MAX_PWM)/2);
  lastDirection = RELEASE;
  Serial.println("RELEASE");

  
}


void loop() {

  Serial.print(x_val, DEC);
  Serial.print(" ");
  Serial.print(y_val, DEC);
  Serial.print(" ");

  Serial.println();

  
  uint16_t _servo1value = map(x_val, x_min, x_max, SERVO1_MIN_PWM, SERVO1_MAX_PWM);
  ledcWrite(8, _servo1value);
  Serial.print("SERVO1: ");
  Serial.println(_servo1value);

  
  if ( abs(y_val - y_center) < 10)
  {
    //STOP
    stop();
  }
  else if (y_val > y_center)
  {
    //FORWARD
    if (lastDirection == BACKWARD)//avoid short circuit of half-bridge
    {
      stop();
      delay(500);
    }
    uint16_t _speed = y_val - y_center;
    
    uint8_t _pwmspeed = map(_speed, 0, y_max - y_center, 0, 255);
    ledcWrite(1, _pwmspeed);
    ledcWrite(2, _pwmspeed);
    ledcWrite(3, _pwmspeed);
    ledcWrite(4, _pwmspeed);

    run(1, FORWARD);
    delay(45);
    run(2, FORWARD);
    delay(45);
    run(3, FORWARD);
    delay(45);
    run(4, FORWARD); 
    delay(45);
    lastDirection = FORWARD;
  }
  else if (y_val < y_center)
  {
    //BACKWARD
    if (lastDirection == FORWARD)//avoid short circuit of half-bridge
    {
      stop();
      delay(500);
    }
    uint16_t _speed = y_center - y_val;
   
    uint8_t _pwmspeed = map(_speed, 0, y_center, 0, 255);
    ledcWrite(1, _pwmspeed);
    ledcWrite(2, _pwmspeed);
    ledcWrite(3, _pwmspeed);
    ledcWrite(4, _pwmspeed);
    run(1, BACKWARD);
    delay(45);
    run(2, BACKWARD);
    delay(45);
    run(3, BACKWARD);
    delay(45);
    run(4, BACKWARD);
    lastDirection = BACKWARD;

  }

  // If the flag "doConnect" is true then we have scanned for and found the desired
  // BLE Server with which we wish to connect.  Now we connect to it.  Once we are
  // connected we set the connected flag to be true.
  if (doConnect == true) {
    if (connectToServer()) {
      Serial.println("We are now connected to the BLE Server.");
    } else {
      Serial.println("We have failed to connect to the server; there is nothin more we will do.");
    }
    doConnect = false;
  }

  // If we are connected to a peer BLE Server, update the characteristic each time we are reached
  // with the current time since boot.
  if (connected) {
    //String newValue = "Time since boot: " + String(millis() / 1000);
    //Serial.println("Setting new characteristic value to \"" + newValue + "\"");

    // Set the characteristic's value to be the array of bytes that is actually a string.
    //pRemoteCharacteristic->writeValue(newValue.c_str(), newValue.length());
  } else if (doScan) {
    //STOP
    stop();
    Serial.println("RELEASE BECAUSE NO BLE CONNECTION");

    BLEDevice::getScan()->start(0);  // this is just example to start scan after disconnect, most likely there is better way to do it in arduino
  }
  delay(200);

}