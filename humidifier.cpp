/*//////////////////////////////////////////////////////////////////////////////
// Developer - Jagrut Jadhav
//   __              __  ___                       __     ___    ___  __  
//  /__`  |\/|  /\  |__)  |     |__| |  |  |\/| | |  \ | |__  | |__  |__) 
//  .__/  |  | /~~\ |  \  |     |  | \__/  |  | | |__/ | |    | |___ |  \ 
//                                                                        
 *//////////////////////////////////////////////////////////////////////////////
#include <Arduino.h>
#include <MapleFreeRTOS821.h>   //free_RTOS kernel
#include <stdint.h>             //header for standard int
#include "DHT.h"                //DHT11 sensor library

//Defining pin names and the module numbers
#define DHTPIN PA0    
#define LED PC14 
#define HUMIDIFIER PB11
#define MOTION PB15
#define DHTTYPE DHT11
#define _SSID_ "XXXXXXXXXXXXX"
#define password "XXXXXXXXXXXXX"
#define write_key "XXXXXXXXXXXXXXXXX"     //Jagrut API key
#define read_key "XXXXXXXXXXXXXXXX"      //Jagrut API key


//Creating objects to the class lib
DHT dht(DHTPIN, DHTTYPE);

//Task handle defines
TaskHandle_t humidity_handle = NULL;
TaskHandle_t thinkspeak_send_handle = NULL;
TaskHandle_t thinkspeak_recv_handle = NULL;
TaskHandle_t mist_handle = NULL;
TaskHandle_t motion_handle = NULL;
TaskHandle_t on_off_handle = NULL;

//Initiating values to be shared between tasks.
uint8_t humid_val =0;
uint8_t set_humid_val;
bool status_mist = 0;
bool motion_val;
bool room_idle = false;
bool on_off = true;

////////////////////Main code //////////////////////////////////////////////////////////////
/*
 * Function creates tasks and runs the scheduler.
 * All process is done once in the program.
 */
void setup() {
  // Creating tasks 
  xTaskCreate(humidity_task, "task_1", 200, (void*) 0, tskIDLE_PRIORITY, &humidity_handle);                       
  xTaskCreate(thinkspeak_send_task, "task_2", 300, (void*) 0, tskIDLE_PRIORITY, &thinkspeak_send_handle);
  xTaskCreate(thinkspeak_recv_task, "task_3", 300, (void*) 0, tskIDLE_PRIORITY, &thinkspeak_recv_handle );
  xTaskCreate(mist_task, "task_4", 300, (void*) 0, tskIDLE_PRIORITY, &mist_handle );
  xTaskCreate(motion_task, "task_5", 300, (void*) 0, tskIDLE_PRIORITY, &motion_handle );
  xTaskCreate(on_off_task, "task_5", 300, (void*) 0, tskIDLE_PRIORITY, &on_off_handle );
  vTaskStartScheduler();       //Start the scheduler
  wifi_init(); //Initialize wifi and connecting ESP8266 to the wifi
  vTaskSuspend(mist_handle);   //Suspend the mist maker task until the data is fetched from the server
}

//////////////////////////////////Tasks functions ////////////////////////////////////////
/*
 *This Task is used to get the On and Off status from thinkspeak. 
 *The data is stored into a status flag.
 *The task first suspends the thinkspeak_recv handle task to get the data.
 *Once the data is fetched, this task resumes the task back.
 */
void on_off_task (void *off)
{
 uint8_t button_on_off;
 Serial2.begin(115200); // ESP8266 UART data communication RX-A3 and TX-A2
 while(1){
  vTaskSuspend(thinkspeak_recv_handle);
   vTaskDelay(1000); 
   button_on_off = tcp_thingspeak_recv('3');
   vTaskDelay(2000);
   if (button_on_off == 10)
     on_off = true;
   else if (button_on_off == 20)
     on_off = false;
  Serial.print("//////////////////////////////On off value is = ");
  Serial.println(button_on_off); 
  vTaskResume(thinkspeak_recv_handle);
  
  }
}
/*
 *This Task is used to see if there is any motion in the room. 
 *The data is fetched from PIR sensor.
 */
void motion_task (void *motion)
{
  uint32_t count=0;
  Serial.begin(115200);
  pinMode(MOTION, INPUT);
  while(1){
    vTaskDelay (1000);
    motion_val = digitalRead(MOTION);
    if (motion_val == HIGH)
    {
    Serial.println("Motion Detected    ");
    room_idle = false;
    count=0;
    }
    else
    {
    count++;
    Serial.println("Motion not detected");
    if (count >= 30)  //use 300 for 5 minuits
    {
      room_idle = true;
    }
   }
  }
}
/*
 *This Task is used to get the humidity data from DHT11 Sensor. 
 *Once the data is fetched, it shares data to the thinkspeak_send task.
 */
void humidity_task (void *h)
{
  Serial.begin(115200);
  dht.begin();
  while(1){
    humid_val = dht.readHumidity();
    Serial.print("Humidity: ");
    Serial.println(humid_val);
    vTaskSuspend(thinkspeak_send_handle);
    vTaskDelay(20000);
    vTaskResume(thinkspeak_send_handle);
    vTaskDelay(2000);
  }
}
/*
 *This Task is used to send the humidity data to the server. 
 *Once the data is fetched in humidity task, it sends the data to the thinkspeak server every 20 seconds.
 */
void thinkspeak_send_task (void *tcp_s)
{
  Serial.begin(115200);
  Serial2.begin(115200); // ESP8266 UART data communication RX-A3 and TX-A2
  //wifi_init();
  while(1){
    //Serial.println(humid_val);
    vTaskDelay(10000);
    tcp_thingspeak_send(humid_val);  
  }
}
/*
 *This Task is used to receieve the humidity data from the server set by the user. 
 *Once the data is fetched from the server every 4 seconds.
 */
void thinkspeak_recv_task (void *tcp_r)
{
  uint8_t humid;
  Serial.begin(115200);
  Serial2.begin(115200); // ESP8266 UART data communication RX-A3 and TX-A2
   
  while(1){    
    vTaskSuspend(on_off_handle);
    vTaskDelay(2000); 
    Serial.println(status_mist);
    vTaskDelay(2000);
    humid = tcp_thingspeak_recv('1');
    if (humid > 5 && humid_val > 10 )
      vTaskResume(mist_handle);
    if (humid < 100)
    {
      set_humid_val = humid;
    }
    Serial.print("Humidity value from User = ");
    Serial.println(set_humid_val);
    vTaskResume(on_off_handle);
  }
}
/*
 *This Task controls the mist based on sensors data and user inputs. 
 *This task is made to run once and then suspends itself until next conditon is satisfied.
 */
void mist_task (void *val_humid)
{
  Serial.begin(115200);
  Serial2.begin(115200); // ESP8266 UART data communication RX-A3 and TX-A2
  pinMode(HUMIDIFIER, OUTPUT);
  pinMode(LED, OUTPUT);
  digitalWrite(HUMIDIFIER,HIGH);
  while(1){
    vTaskDelay(500);
    if (on_off == false && status_mist == true)
    {
      humidifier_toggle();
    }
    else if (on_off == true)
    {
      if (room_idle == true && status_mist == true)
      {
       humidifier_toggle();
      }
      else if (room_idle == false)
      {
       if (set_humid_val > humid_val && status_mist == true)
        vTaskSuspend(NULL);

       else if (set_humid_val <= humid_val && status_mist == true)
        {
         humidifier_toggle();
        }
       else if (set_humid_val > humid_val && status_mist == false)
        {
          humidifier_toggle();
        }
      }
    }
      Serial.print("inside mist task");
      Serial.println(status_mist);    
  }
}
////////////////////////////////Functions for the program/////////////////////////////////

void wifi_init()  //Function to intialize wifi
{
  vTaskDelay(500);
  Serial2.println("AT");
  vTaskDelay(500);
  Serial2.println("AT+CWMODE=3");
  vTaskDelay(500);
  Serial2.print("AT+CWJAP=\"");   //connecing ESP to the wifi
  Serial2.print(_SSID_);
  Serial2.print("\",\"");
  Serial2.println(password);
  Serial2.print("\"");
  vTaskDelay(4000);
  Serial2.println("AT+CIPMUX=0");

}
void tcp_thingspeak_send(int val)   //Sending data to the thinkspeak server
{
  vTaskDelay(500);
  Serial2.println("AT+CIPSTART=\"TCP\",\"api.thingspeak.com\",80");
  vTaskDelay(500);
  Serial2.println("AT+CIPSEND=64");
  vTaskDelay(500);
  Serial2.print("GET /update?api_key=");
  Serial2.print(write_key);
  Serial2.print("&field1=");
  Serial2.println(val);
  vTaskDelay(1000);
  Serial2.println("AT+CIPCLOSE");
  vTaskDelay(500);
  Serial2.println("AT+CIPCLOSE");
  vTaskDelay(500);
}
int tcp_thingspeak_recv(char field)   //Receieveing data from thinkspeak server
{
  char csv_data[100];
  int count = 0;
  vTaskDelay(500);
  Serial2.println("AT+CIPSTART=\"TCP\",\"api.thingspeak.com\",80");
  vTaskDelay(500);
  Serial2.println("AT+CIPSEND=100");
  vTaskDelay(500);
  Serial2.print("GET https://api.thingspeak.com/channels/1345246/fields/");
  Serial2.print(field);
  Serial2.print(".csv?api_key=");  //channel code for Jagrut - 1016595 //field 1
  Serial2.print(read_key);
  Serial2.println("&results=1");
  vTaskDelay(200);
  Serial2.println("AT+CIPCLOSE");
  vTaskDelay(100);
  while(Serial2.available())
    {
      csv_data[count] = char(Serial2.read());
      count++;  
    }
  int data = (((int(csv_data[52]) - 48)* 10)+ (int(csv_data[53])-48));
  return data;
}

void humidifier_toggle()   //toggle the mist maker output
{
  digitalWrite(HUMIDIFIER,LOW);
  vTaskDelay(50);
  status_mist = not(status_mist);
  Serial.println(status_mist);
  digitalWrite(HUMIDIFIER,HIGH);
  if (status_mist == true)
  {
    digitalWrite(LED,HIGH);
  }
  else 
    digitalWrite(LED,LOW);
  vTaskSuspend(NULL);
}
void loop() {
  // put your main code here, to run repeatedly:

}
