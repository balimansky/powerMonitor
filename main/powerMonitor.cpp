#include <Wire.h>
#include <Adafruit_ADS1015.h>
#include <U8g2lib.h>
#include <WiFi.h>
#include <ThingSpeak.h>

Adafruit_ADS1115 ads;  /* Use this for the 16-bit version */

const char *SSID = "FiOS-KNRXW";
const char *PASSWORD = "flux2383gap9255hug";

WiFiClient client;
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);

const char *myWriteAPIKey = "185R1CPLIR9KSC26";
unsigned long myChannel = 623462;
unsigned long lastUpdate;

const float FACTOR = 0.10;
const float multiplier = 0.1875;
unsigned long counter;

struct currentStruct
{
  float voltage;
  float current;
  float sum;
  float power;
} current[2];

void setup(void)
{
  Serial.begin(115200);
  u8g2.begin();
  u8g2.setFont(u8g2_font_6x10_tf);
  
  // The ADC input range (or gain) can be changed via the following
  // functions, but be careful never to exceed VDD +0.3V max, or to
  // exceed the upper and lower limits if you adjust the input range!
  // Setting these values incorrectly may destroy your ADC!
  //                                                                ADS1015  ADS1115
  //                                                                -------  -------
  // ads.setGain(GAIN_TWOTHIRDS);  // 2/3x gain +/- 6.144V  1 bit = 3mV      0.1875mV (default)
  // ads.setGain(GAIN_ONE);        // 1x gain   +/- 4.096V  1 bit = 2mV      0.125mV
  // ads.setGain(GAIN_TWO);        // 2x gain   +/- 2.048V  1 bit = 1mV      0.0625mV
  // ads.setGain(GAIN_FOUR);       // 4x gain   +/- 1.024V  1 bit = 0.5mV    0.03125mV
  // ads.setGain(GAIN_EIGHT);      // 8x gain   +/- 0.512V  1 bit = 0.25mV   0.015625mV
  // ads.setGain(GAIN_SIXTEEN);    // 16x gain  +/- 0.256V  1 bit = 0.125mV  0.0078125mV
  
  ads.begin();
  WiFi.mode(WIFI_STA);

  connectToWiFi(SSID, PASSWORD);
  ThingSpeak.begin(client);

  lastUpdate = millis();
}

void printMeasure(String prefix, float value, String postfix)
{
  Serial.print(prefix);
  Serial.print(value, 3);
  Serial.println(postfix);
}

void loop(void)
{
  getCurrent();

  String s1, s2, s3, s4;
  s1 = String("L1:") + String(current[0].current) + String(" Amps") + String("  :") + String(counter);
  s2 = String("L1:") +String(current[0].power) + String(" Watts");
  s3 = String("L2:") +String(current[1].current) + String(" Amps") + String("  :") + String(counter);
  s4 = String("L2:") +String(current[1].power) + String(" Watts");


  Serial.println (String(counter) + " " + s1 + " " + s2);
  Serial.println (String(counter) + " " + s3 + " " + s4);
  
  u8g2.clearBuffer();
  u8g2.setCursor(0,10);
  u8g2.print(s1);
  u8g2.setCursor(0,20);
  u8g2.print(s2);
  u8g2.setCursor(0,30);
  u8g2.print(s3);
  u8g2.setCursor(0,40);
  u8g2.print(s4);
  u8g2.sendBuffer();  

  if (millis() - lastUpdate > 20000)
    {
      ThingSpeak.setField(1, current[0].current);
      ThingSpeak.setField(2, current[0].power);  
      ThingSpeak.setField(3, current[1].current);
      ThingSpeak.setField(4, current[1].power);  
    
      int x = ThingSpeak.writeFields(myChannel, myWriteAPIKey);
      if(x == 200){
	Serial.println("Channel update successful.");
      }
      else{
	Serial.println("Problem updating channel. HTTP error code " + String(x));
      }
      lastUpdate = millis();
    }

  counter++;
}

void getCurrent()
{
  long timep = millis();
  int counter = 0;

  current[0].sum = 0;
  current[1].sum = 0;
  
  while(millis() - timep < 1000)
    {
      current[0].voltage = ads.readADC_Differential_0_1() * multiplier;
      current[1].voltage = ads.readADC_Differential_2_3() * multiplier;
      
      current[0].current = current[0].voltage * FACTOR;
      current[1].current = current[1].voltage * FACTOR;
      
      current[0].sum += sq(current[0].current);
      current[1].sum += sq(current[1].current);
      
      counter += 1;
    }

  current[0].current = sqrt(current[0].sum/counter);
  current[1].current = sqrt(current[1].sum/counter);

  current[0].power = 120 * current[0].current;
  current[1].power = 120 * current[1].current;
}

void connectToWiFi(const char * ssid, const char * pwd)
{
  u8g2.clearBuffer();          // clear the internal memory
  u8g2.setCursor(0,10);
  u8g2.print("Connecting to WiFi network: " + String(ssid));
  u8g2.sendBuffer();          // transfer internal memory to the display
  
  Serial.println("Connecting to WiFi network: " + String(ssid));

  WiFi.begin(ssid, pwd);

  while (WiFi.status() != WL_CONNECTED)
    {
      delay(500);
      Serial.print(".");    
    }

  Serial.println();
  Serial.println("WiFi connected!");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  u8g2.clearBuffer();
  u8g2.setCursor(0,10);
  u8g2.print("WiFi connected at:" + String(WiFi.localIP()));
  u8g2.sendBuffer();
}

void requestURL(const char * host, uint8_t port)
{
  Serial.println("Connecting to domain: " + String(host));

  // Use WiFiClient class to create TCP connections
  WiFiClient client;
  if (!client.connect(host, port))
    {
      Serial.println("connection failed");
      return;
    }
  Serial.println("Connected!");


  // This will send the request to the server
  client.print((String)"GET / HTTP/1.1\r\n" +
               "Host: " + String(host) + "\r\n" +
               "Connection: close\r\n\r\n");
  unsigned long timeout = millis();
  while (client.available() == 0)
    {
      if (millis() - timeout > 5000)
	{
	  Serial.println(">>> Client Timeout !");
	  client.stop();
	  return;
	}
    }

  // Read all the lines of the reply from server and print them to Serial
  while (client.available())
    {
      String line = client.readStringUntil('\r');
      Serial.print(line);
    }

  Serial.println();
  Serial.println("closing connection");
  client.stop();
}
