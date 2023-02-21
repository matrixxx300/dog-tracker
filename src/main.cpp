/// main.cpp
/// Main dog-tracker file
/// Created on: 21 Feb 2023
/// Author: Mateusz Sznejkowski

//////////////////////////////////
//					 INCLUDES 					//
//////////////////////////////////
#include <Arduino.h>
#include <Serial3.h>
#include <TinyGPS++.h>

//////////////////////////////////
//			MACROS AND DEFINES			//
//////////////////////////////////
#define gps_serial Serial1
#define sim_serial Serial3

#define GPS_BAUDRATE 9600
#define SIM_BAUDRATE 9600

#define PHONE_NUMER_SIZE 11
#define BUFFER_SIZE 128

//////////////////////////////////
//					 TYPEDEFS 					//
//////////////////////////////////
typedef struct
{
  double latitude;  /// Latitude in degrees
  double longitude; /// Longitude in degrees
  uint32_t date;    /// Raw date in DDMMYY format
  uint32_t time;    /// Raw time in HHMMSSCC format
} Packet;

//////////////////////////////////
//           VARIABLES          //
//////////////////////////////////
TinyGPSPlus gps; /// GPS

unsigned long previousMillis = 0;
unsigned long interval = 1000;
bool is_data_requested = false;
bool gps_ready = false;
Packet packet = {0};
char phone_number[PHONE_NUMER_SIZE + 1] = "";

//////////////////////////////////
//	STATIC FUNCTION PROTOTYPES	//
//////////////////////////////////
static void gps_serial_handler(void);
static void sim_serial_handler(void);
static void sim_send_sms(void);
static void sim_send_command(const char *command, uint8_t command_size, const char *expected_response, uint8_t expected_response_size);
static void init_sim(void);
static void start_serial_with_pc(void);

//////////////////////////////////
//       GLOBAL FUNCTIONS       //
//////////////////////////////////
/**
 * @brief Configuration
 */
void setup(void)
{
  start_serial_with_pc();
  init_sim();
  gps_serial.begin(GPS_BAUDRATE);
  Serial.println("Started!!!");
}

/**
 * @brief Main loop function
 */
void loop(void)
{
  unsigned long currentMillis = millis();

  if (currentMillis - previousMillis > interval)
  {
    previousMillis = currentMillis;

    if (is_data_requested) // && gps_ready)
    {
      is_data_requested = false;
      sim_send_sms();
    }
  }

  gps_serial_handler();
  sim_serial_handler();
}

//////////////////////////////////
//       STATIC FUNCTIONS       //
//////////////////////////////////
/**
 * @brief GY-GPS6MV2 serial handler
 */
static void gps_serial_handler(void)
{
  while (gps_serial.available() > 0)
  {
    gps.encode(gps_serial.read());
    if (gps.location.isUpdated())
    {
      packet.latitude = gps.location.lat();
      packet.longitude = gps.location.lng();
      packet.date = gps.date.value();
      packet.time = gps.time.value();
      gps_ready = true;
      Serial.println("Location acquired!");
    }
  }
}

/**
 * @brief SIM800L serial handler
 */
static void sim_serial_handler(void)
{
  while (sim_serial.available() > 0)
  {
    String sms = sim_serial.readString();

    if (sms.indexOf("Start") > 0)
    {
      Serial.println("Requested location!");
      strncpy(phone_number, sms.c_str() + 10, PHONE_NUMER_SIZE);
      is_data_requested = true;
    }
    else
    {
      Serial.println(sms);
    }
  }
}

/**
 * @brief Sends SMS via SIM800L module to to the phone number from which the location request came
 */
static void sim_send_sms(void)
{
  char request[BUFFER_SIZE] = "";
  char message[BUFFER_SIZE] = "";

  // SendS a command to SIM800L module to make it ready to send a message
  sprintf(request, "AT+CMGS=\"+%s\"", phone_number);
  sim_serial.println(request);
  delay(50);
  while (sim_serial.available())
  {
    Serial.write(sim_serial.read());
  }

  // adds text to SMS message
  sprintf(message, "http://www.google.com/maps/place/%lf,%lf", packet.latitude, packet.longitude);
  sim_serial.print(message);
  delay(50);
  while (sim_serial.available())
  {
    Serial.write(sim_serial.read());
  }

  sim_serial.write(26); // write 26 to SIM800L to end sending SMS command

  Serial.println(request);
  Serial.println(message);
}

/**
 * @brief Sends command to SIM800L module
 */
static void sim_send_command(const char *command, uint8_t command_size, const char *expected_response, uint8_t expected_response_size)
{
  sim_serial.println(command);
  sim_serial.flush();
  delay(50);

  // Reads serial character by character
  char text[BUFFER_SIZE] = "";
  uint8_t i = 0;
  while (sim_serial.available())
  {
    char c = sim_serial.read();
    text[i++] = c;
  }

  // /r/n<response>/r/n
  // adding 2 characters to omit the newline character
  if (strncmp(text + 2, expected_response, expected_response_size) != 0)
  {
    Serial.println("SIM800L COMMAND ERROR!");
    Serial.print("Command: ");
    Serial.println(command);
    Serial.print("Expected response: ");
    Serial.println(expected_response);
    Serial.print("Received response: ");
    text[i - 1] = '\0';
    text[i - 2] = '\0';
    i -= 2;
    Serial.println(text + 2);
  }
}

/**
 * @brief Initializes SIM800L module
 */
static void init_sim(void)
{
  Serial.println("\rInitializing SIM800L...");
  Serial.println("Setting up Serial2...");
  sim_serial.begin(SIM_BAUDRATE);

  sim_send_command("ATE0", strlen("ATE0"), NULL, 0);                                      // Disable ECHO
  sim_send_command("AT", strlen("AT"), "OK", strlen("OK"));                               // Once the handshake test is successful, it will back to OK
  sim_send_command("AT+CMGF=1", strlen("AT+CMGF=1"), "OK", strlen("OK"));                 // Configuring TEXT mode
  sim_send_command("AT+CNMI=1,2,0,0,0", strlen("AT+CNMI=1,2,0,0,0"), "OK", strlen("OK")); // Decides how newly arrived SMS messages should be handled
}

/**
 * @brief Starts serial with PC
 */
static void start_serial_with_pc(void)
{
  // Wait up to 10 seconds for Serial (= USBSerial) port to come up. (Usual wait is 0.5 second.)
  unsigned long start_serial_time = millis();
  while (!Serial && (millis() - start_serial_time < 10000))
    ;
  // Serial is now up.

  Serial.println("5 second initial delay");
  // Time to start the IDE serial monitor or to upload new firmware
  for (int8_t i = 5; i > -1; i--)
  {
    Serial.printf("\rStartup delay: %d", i);
    delay(1000);
  }
}

/// (C) COPYRIGHT Mateusz Sznejkowski ///