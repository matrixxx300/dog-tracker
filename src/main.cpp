#include <Arduino.h>
#include <Serial3.h>
#include <TinyGPS++.h>

#define gps_serial Serial1
#define sim_serial Serial3

TinyGPSPlus gps; /// GPS

typedef struct
{
  double latitude;  /// Latitude in degrees
  double longitude; /// Longitude in degrees
  uint32_t date;    /// Raw date in DDMMYY format
  uint32_t time;    /// Raw time in HHMMSSCC format
} Packet;

static void gps_serial_handler(void);
static void sim_serial_handler(void);
static void sim_send_sms(void);
static void sim_send_command(const char *command, uint8_t command_size, const char *expected_response, uint8_t expected_response_size);
static void init_sim(void);
static void start_serial_with_pc(void);

bool gps_ready = false;
Packet packet = {0};
unsigned long previousMillis = 0;
unsigned long interval = 1000;
bool is_data_requested = false;
char phone_number[12] = "";

/**
 * @brief Configuration
 */
void setup()
{
  start_serial_with_pc();
  init_sim();
  gps_serial.begin(9600);
  Serial.println("Started!!!");
}

/**
 * @brief Main loop function
 */
void loop()
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
      strncpy(phone_number, sms.c_str() + 10, 11);
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
  char request[128] = "";
  char message[128] = "";

  sprintf(request, "AT+CMGS=\"+%s\"", phone_number);
  sim_serial.println(request);
  delay(50);
  while (sim_serial.available())
  {
    Serial.write(sim_serial.read());
  }

  sprintf(message, "http://www.google.com/maps/place/%lf,%lf", packet.latitude, packet.longitude);
  sim_serial.print(message);
  delay(50);
  while (sim_serial.available())
  {
    Serial.write(sim_serial.read());
  }

  sim_serial.write(26);

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
  char text[64] = "";
  uint8_t i = 0;
  while (sim_serial.available())
  {
    char c = sim_serial.read();
    text[i++] = c;
  }

  // /r/n<response>/r/n
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
  sim_serial.begin(9600);

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

// char char_array[7] = "";
// strncpy(char_array, sms.c_str() + 49, 6);
// if (strncmp(sms.c_str() + 49, "Start", 5) == 0)

// uint16_t latitude_deg;  /// Latitude in degrees
// uint32_t latitude_billionths; /// Longitude in degrees
// uint16_t longitude_deg;
// uint32_t longitude_billionths;

// packet.latitude_deg = gps.location.rawLat().deg;
// packet.latitude_billionths = gps.location.rawLat().billionths;
// packet.longitude_deg = gps.location.rawLng().deg;
// packet.longitude_billionths = gps.location.rawLng().billionths;

// // Raw speed in 100ths of a knot (i32)
// Serial.print("Raw speed in 100ths/knot = ");
// Serial.println(gps.speed.value());
// // Speed in knots (double)
// Serial.print("Speed in knots/h = ");
// Serial.println(gps.speed.knots());
// // Speed in miles per hour (double)
// Serial.print("Speed in miles/h = ");
// Serial.println(gps.speed.mph());
// // Speed in meters per second (double)
// Serial.print("Speed in m/s = ");
// Serial.println(gps.speed.mps());
// // Speed in kilometers per hour (double)
// Serial.print("Speed in km/h = ");
// Serial.println(gps.speed.kmph());
// // Raw course in 100ths of a degree (i32)
// Serial.print("Raw course in degrees = ");
// Serial.println(gps.course.value());
// // Course in degrees (double)
// Serial.print("Course in degrees = ");
// Serial.println(gps.course.deg());
