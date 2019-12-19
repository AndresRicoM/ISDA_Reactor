/* 

██╗███████╗██████╗  █████╗     ██████╗ ███████╗ █████╗  ██████╗████████╗ ██████╗ ██████╗ 
██║██╔════╝██╔══██╗██╔══██╗    ██╔══██╗██╔════╝██╔══██╗██╔════╝╚══██╔══╝██╔═══██╗██╔══██╗
██║███████╗██║  ██║███████║    ██████╔╝█████╗  ███████║██║        ██║   ██║   ██║██████╔╝
██║╚════██║██║  ██║██╔══██║    ██╔══██╗██╔══╝  ██╔══██║██║        ██║   ██║   ██║██╔══██╗
██║███████║██████╔╝██║  ██║    ██║  ██║███████╗██║  ██║╚██████╗   ██║   ╚██████╔╝██║  ██║
╚═╝╚══════╝╚═════╝ ╚═╝  ╚═╝    ╚═╝  ╚═╝╚══════╝╚═╝  ╚═╝ ╚═════╝   ╚═╝    ╚═════╝ ╚═╝  ╚═╝

ISDA Reactor - MAS.552
Andres Rico - MIT Media Lab - aricom@mit.edu
Nicolas Ayoub - Harvard Graduate School Of Design - nayoub@gsd.harvard.edu

Main controller code for the ISDA reactor prototype. This code runs on a teensy 3.2 (See electric diagrams for connection details)
The protoytpe is capable of regulating atmospheric CO2 within the tank, filtering incoming water from pollutants and bacteria, knowing 
water levels, and regulating water temperature.

Possible improvements to this code:

- Being able to adjust levels for different variables at the same time: The current version is limited to adjusting only one variable at a time.
If levels of a certain variable fall out of boundaries while the tank is adjusting another one, this will not be adjusted until other variable is adjusted. 

- Adding a water pump or water valve to the water filter could help to regulate water levels in a more autonomous manner. 

- 

CC - Andres Rico - MIT Media Lab
   
*/

#include <OneWire.h> //I2C communication is required by temperature and eCO2 sensors. 
#include <DallasTemperature.h> //Library for correct operation of temperature sensor. 
#include <Wire.h> //I2C communication is required by temperature and eCO2 sensors.
#include "Adafruit_SGP30.h" //For communication with eCO2 sensor. 
#include <Adafruit_NeoPixel.h> //For controlling LED ring. 

#ifdef __AVR__
  #include <avr/power.h>
#endif

#define PIN 8 //Pin ised to control LED ring. This pin is connected to DI on any adafruit neopixel device. 
#define ONE_WIRE_BUS A9 //Analog pin used as input pin for temperature system. 

int led_num = 24;

Adafruit_NeoPixel strip = Adafruit_NeoPixel(led_num, PIN, NEO_GRB + NEO_KHZ800);

Adafruit_SGP30 sgp; //Used for eCO2 sensor. 

OneWire oneWire(ONE_WIRE_BUS); //Used for communication with temperature sensor. 

DallasTemperature sensors(&oneWire);

bool calibration = false; //Variable for eCO2 calibration. 
int eCo2ppm;

const int trigPin = 2; //Declare pin for trigger US unit. 
const int echoPin = 3; //Declare pin for receiving US unit. 
const int circulation_fan = 4; //Pin enables drivers for running upper fans for CO2 extraction. 
const int thermal_fan = 5; //Pin enables driver to run lower extraction fan for unit temperature regulation. 
const int peltier_hot = 6; //Pin enables driver to polarize peltier to cool water down. 
const int peltier_cold = 7; //Pin enables driver to polarize peltier to heat water up. 

float temperature, co2, water_level; //Main variables for storing values for temperature, co2, and water level in tank. 

bool temp_status, co2_status, water_status; //Control boolean variables for checking status in tank. This variables will be used by check_tank function. 

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  Serial1.begin(115200); 
  Wire.begin();
  Wire.setSDA(18);
  Wire.setSCL(19);
  sensors.begin(); 

  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);
  pinMode(circulation_fan, OUTPUT);
  pinMode(thermal_fan, OUTPUT);
  pinMode(peltier_hot, OUTPUT);
  pinMode(peltier_cold, OUTPUT);

  #if defined (__AVR_ATtiny85__)
    if (F_CPU == 16000000) clock_prescale_set(clock_div_1);
  #endif

  strip.begin();
  strip.show();
  
  if (! sgp.begin()){
    Serial.println("eCO2 Sensor not found :(");
    Serial1.println("eCO2 Sensor not found :(");
    //while (1);
  }
  Serial.println("Found SGP301");
  Serial1.println("Found SGP301");

  Serial.println("Calibrating eCo2 Sensor...");
  Serial1.println("Calibrating eCo2 Sensor...");
  
  if (! sgp.IAQmeasure()) {
    Serial.println("Measurement failed");
    Serial1.println("Measurement failed");
    return;
  }
  eCo2ppm = sgp.eCO2;
  //Serial.print("eCO2 "); Serial.print(sgp.eCO2); Serial.println(" ppm");
  while (eCo2ppm == 400) {
    if (! sgp.IAQmeasure()) {
    Serial.println("Measurement failed");
    Serial1.println("Measurement failed");
    return;
  }
  calibrating_pulse();
  eCo2ppm = sgp.eCO2;
  }
  Serial.println("eCo2 Sensor has been calibrated!");
  Serial1.println("eCo2 Sensor has been calibrated!");
}

void loop() {
  
  get_all_sensors();
  check_tank();
  lightson();

  while (!temp_status) { //Corrects temperature if check_tank returns a bad temperature state. 
    while (temperature > 28) {
      get_all_sensors(); 
      coollights();
      cool_water();
      Serial.println("Cooling Water...");
      Serial1.println("Cooling Water...");
    }
    stop_thermal();
    while (temperature < 23) { //Corrects temperature if check_tank returns a bad temperature state. 
      get_all_sensors();
      heatlights();
      heat_water();
      Serial.println("Heating Water...");
      Serial1.println("Heating Water...");
    }
    stop_thermal();
    check_tank();
  }

  while (!co2_status) { //Corrects co2 levels if check_tank returns a bad co2 state. 
    while (co2 > 1000){
      get_all_sensors();
      extract_co2();
      Serial.println("Extracting CO2...");
      Serial1.println("Extracting CO2...");
    }
    stop_extraction(); 
    check_tank();
  }

  while (!water_status) { //Alerts if water levels are too high.
    get_all_sensors();
    check_tank();
    water_alert(); //Flashes tank in red. 
    Serial.println("Water levels are HIGH, EMPTY TANK!");
    Serial1.println("Water levels are HIGH, EMPTY TANK!");
  }

}

//Function returns current tank water temperature in Celcius. 
float get_temperature() { 
  sensors.requestTemperatures();
  float temp_value = sensors.getTempCByIndex(0);
  //Serial.println(temp_value);
  return temp_value;
}

//fucntion returns current CO2 levels in ppm. 
int get_co2 () {
  int co2_value = sgp.eCO2;

  if (! sgp.IAQmeasure()) {
    Serial.println("CO2 Measurement failed");
    Serial1.println("CO2 Measurement failed");
    return;
  }
  
  //Serial.println(co2_value);
  return co2_value;
}

long get_water_level() { //Function returns water level from US sensor. Should be kept at a max of 4. 
  //The empty tank should yield values of 12-14. 
  long milis = get_duration();
  long water_level = milis / 29 / 2;
  //Serial.println(water_level);
  return water_level;
}

long get_duration() { //Used for US distance calculation. Returns duration of pulse. 
   digitalWrite(trigPin, LOW);
   delayMicroseconds(2);
   digitalWrite(trigPin, HIGH);
   delayMicroseconds(10);
   digitalWrite(trigPin, LOW);
   long duration = pulseIn(echoPin, HIGH);
   return duration;
}

void extract_co2() { //Turns on both of the top fans.
  digitalWrite(circulation_fan, HIGH);
}

void stop_extraction () { //Stops top fans. 
  digitalWrite(circulation_fan, LOW);
}

void heat_water() { //Polarizes peltier to have heating side towards acrylic. 
  digitalWrite(peltier_hot, HIGH);
  digitalWrite(thermal_fan, HIGH);
}

void cool_water() { //Polarizes peltier to have cooling side towards acrylic. 
  digitalWrite(peltier_cold, HIGH);
  digitalWrite(thermal_fan, HIGH);
}

void stop_thermal() { //Turns the peltier off. 
  digitalWrite(peltier_cold, LOW);
  digitalWrite(peltier_hot, LOW);
  digitalWrite(thermal_fan, LOW);
}

void check_tank(){ //Fucntion checks tank variables. If a variable is not within desirable parameters it will be marked so that the reactor can automatically correct it. 
  
  if (temperature > 28) { //Variables can be modified depending on type of environment that is needed. 
    temp_status = false;
  } else if (temperature < 23) {
    temp_status = false;
  } else {
    temp_status = true;
  }
  
  if (co2 < 1000) {
    co2_status = true;
  } else {
    co2_status = false;
  }

  if (water_level > 4) {
    water_status = true;
  } else {
    water_status = false;
  }
}

//Fucntion reads temperature, water level and co2 levels. 
void get_all_sensors() {
  
  temperature = get_temperature();
  co2 = get_co2();
  water_level = get_water_level();

  //Print all sensor values into serial plotter for live visualization. 
  //Values are printed through tx and GND cables coming out of tank. 
  //Connnect cables to any serial enabeled device for communication with tank. 
  Serial.print(temperature);
  Serial.print(", ");
  Serial.print(co2);
  Serial.print(", ");
  Serial.println(water_level);
  Serial1.print(temperature);
  Serial1.print(", ");
  Serial1.print(co2);
  Serial1.print(", ");
  Serial1.println(water_level);
 
}

void lightsoff() { //Funtion for turning lights off. 
  for (int i = 0; i < led_num; i++) {
    strip.setPixelColor(i, 0, 0, 0); //All RGB values set to 0. 
    delay(10);
  }
  strip.show();
}

void lightson() { //Funtion for turning lights on (White). Indicates that tank is in a normal state. 
  for (int i = 0; i < led_num; i++) {
    strip.setPixelColor(i, 255, 255, 255); //white RGB.
    delay(10);
  }
  strip.show();
}

void heatlights() { //Funtion for turning lights on (Magenta). 
  for (int i = 0; i < led_num; i++) {
    strip.setPixelColor(i, 255, 0, 255); //Magenta RGB.
    delay(10);
  }
  strip.show();
}

void coollights() { //Funtion for turning lights on (Blue). 
  for (int i = 0; i < led_num; i++) {
    strip.setPixelColor(i, 0, 255, 255); //Turquoise RGB
    delay(10);
  }
  strip.show();
}

void water_alert() { //Funtion for turning lights on (Red). 
  for (int i = 0; i < led_num; i++) {
    strip.setPixelColor(i, 255, 0, 0); //Red RGB
    delay(10);
  }
  strip.show();
  delay(300);
  lightsoff();
  delay(300);
}

void calibrating_pulse() { //LED notification function. Pulses green while co2 sensor is calibrating. 
  for (int color = 0; color < 255; color++) {
    for (int led = 0; led < led_num; led++) {
      strip.setPixelColor(led, 0, color, 0);
      delay(1);
    }
    strip.show();
  }
  for (int color = 0; color < 255; color++ ) {
    for (int led = 0; led < led_num; led++) {
      strip.setPixelColor(led, 0, (255 - color), 0);
      delay(1);
    }
    strip.show();
  }
}
