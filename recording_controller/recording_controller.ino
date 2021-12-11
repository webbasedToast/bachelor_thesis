// https://github.com/fabiuz7/melody-player-arduino
#include "melody_player.h"
#include "melody_factory.h"
#include "heltec.h"
#include "Arduino.h"
#include <driver/i2s.h>
#include <WiFi.h>
#include "time.h"
#include "stdio.h"

// Pins needed to configure Microphone and recording process
#define I2S_WS 15
#define I2S_SD 13
#define I2s_SCK 2
#define I2S_PORT I2S_NUM_0
#define I2S_SAMPLE_RATE   (16000)
#define I2S_SAMPLE_BITS   (16)
#define I2S_READ_LEN      (16 * 1024)
#define RECORD_TIME       (13) //in seconds
#define I2S_CHANNEL_NUM   (1)
#define FLASH_RECORD_SIZE (I2S_CHANNEL_NUM * I2S_SAMPLE_RATE * I2S_SAMPLE_BITS / 8 * RECORD_TIME)


// Params for the WiFi Connection
WiFiServer server(80); // Opens a Server on Port 80
const char* ssid = "HCI Lab 2.4GHz";
const char* password = "wearedoingresearch.";
String header; // Stores the HTTP Request

// Timer variables used for client timeouts defined in Millisecs
unsigned long currentTime = millis();
unsigned long previousTime = 0;
const long timeoutTime = 2000;

// Params for the NTP Connection
const char* ntpServer = "europe.pool.ntp.org";
const long gmtOffset_sec = 3600;
const int daylightOffset_sec = 3600;

// MelodyPlayer output set to DAC pin
int buzzerPin = 26; 
MelodyPlayer player(buzzerPin);
MelodyPlayer player2(buzzerPin);
MelodyPlayer player3(buzzerPin);

File file;
char filename[64 + 1];

/**
 *  Generates the name for the recordingfile to be used by accessing an ntp server
 *  and adding current time and date values to the filepath
 */
bool setFilename(char *buffer, size_t buffer_size) {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Failed to obtain time");
  }
  // Print the desired time and date values to a buffer as string
  int err = strftime(buffer, buffer_size, "recording_%Y %B %d-%H-%M-%S.wav", &timeinfo);
  if (err == 0) {
    Serial.println("strftime failed");
    return false;
  }
  return true;
}


/**
 * Setup for the speaker
 * Maybe not necessary due to melodyplayer
 */
const int speaker_output26 = 26; // Speaker output set to DAC pin
const int frequency = 2000; // Initial frequency
const int resolution = 8; // Resolution of the duty cycle in Bit
const int channel = 0; // PWM channel
const int headerSize = 44; // Size for .wav-header
char* i2s_read_buff;
uint8_t* flash_write_buff;


/**
 * Setup of the sketch.
 * Initializes the controllers display, establishes a connection to WLAN and to a NTP Server.
 * Connects to the Microphone.
 * Starts the Webserver.
 */
void setup() {
  Heltec.begin(true/*enables Display*/, false/*disables LoRa*/, true/*enables Serial*/);

  /** 
   * Setup for the speaker
   * Maybe not necessary due to melodyplayer
   */
  pinMode(speaker_output26, OUTPUT);

  Serial.println("Initializing Speaker...");
  if (!ledcSetup(channel, frequency, resolution)) {
    Serial.println("Initialization failed..");
  }
  ledcAttachPin(speaker_output26, channel);

  Heltec.display->clear();
  Heltec.display->setTextAlignment(TEXT_ALIGN_LEFT);
  Heltec.display->setFont(ArialMT_Plain_10);
  Heltec.display->drawString(0, 0, "Connecting to ");
  Heltec.display->drawString(10, 10, ssid);

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer); //Connects to NTP-Server to get current UNIX-Time

  Serial.println("");
  // Print the IP of the Controller to the display
  Heltec.display->drawString(0, 20, "WiFi connected.");
  Heltec.display->drawString(0, 30, "IP address:");
  Heltec.display->drawString(10, 40, WiFi.localIP().toString());
  Heltec.display->display();

  i2sInit();
  buffersInit();

  server.begin();
  Serial.println("Setup() done.");
}


bool run_melody = false;
bool run_whitenoise = false;
bool run_increase = false;


/**
 * Used to establish the connection to a websocket.
 * Stores the data to display the webpage and can print them to the webclient.
 * Acts as a handler for incoming GET-requests to start the Stimulus application and recordings.
 */
void loop() {
  WiFiClient client = server.available(); // Listen for incoming clients

  if (client) {
    Serial.println("Client connected.");
    currentTime = millis();
    previousTime = currentTime;
    String currentLine = "";
    while (client.connected() && currentTime - previousTime <= timeoutTime) {
      currentTime = millis();
      if (client.available()) {
        char c = client.read();
        header += c;
        if (c == '\n') {
          if (currentLine.length()  == 0) {
            client.println("HTTP/1.1 200 OK");
            client.println("Access-Control-Allow-Origin: *");

            // Handle GET requests 
            if (header.indexOf("GET /recordingMelody") >= 0) {
              run_melody = true;
              break;
            }
            else if (header.indexOf("GET /recordingWhiteNoise") >= 0) {
              run_whitenoise = true;
              break;
            }
            else if (header.indexOf("GET /recordingIncrease") >= 0) {
              run_increase = true;
              break;
            }
            

            // Build a string to store webpage in.
            String S = "<!DOCTYPE html><html>";
            S += "<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">";
            S += "<link rel=\"icon\" href=\"data:,\">";
            S += "<style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}";
            S += "h1{color: #0F3376;padding: 2vh;}";
            S += ".button {display: inline-block;background-color: #008CBA; border: none;border-radius: 4px;color: white;padding: 16px 40px;text-decoration: none;font-size: 30px;margin: 2px;cursor: pointer;}";
            S += ".button2 {background-color: #AE270B;}</style></head>";
            // Web Page Heading
            S += "<body><h1>Microcontroller-Interface</h1>";
            S += "<p>Start Recording</p>";
            // Each button starts one distinct soundrecording
            S += "<a href=\"/recordingWhiteNoise\"><button class=\"button\">White Noise</button></a>";
            S += "<a href=\"/recordingMelody\"><button class=\"button\">Mixed</button></a>";
            S += "<a href=\"/recordingIncrease\"><button class=\"button\">Increase</button></a>";
            S += "</body></html>";
            // The HTTP response ends with another blank line
            S += "\n";

            client.println("Content-type: text/html");
            client.print("Content-Length: ");
            client.println(S.length() + 2); // Excess found in a non pipelined read: excess = 2, size = 960, maxdownload = 960, bytecount = 0; so we add "+2" here
            client.println("Connection: close");
            client.println();
            client.println(S);
            
            // Break out of the while loop
            break;
          } else { // if you got a newline, then clear currentLine
            currentLine = "";
          }
        } else if (c != '\r') {  // if you got anything else but a carriage return character,
          currentLine += c;      // add it to the end of the currentLine
        }
      }
    }
    
    if (run_melody) {
      /**
       * Creates a fixed melody (twinkle twinkle little star) and passes it to the MelodyPlayer.
       * Also records the playback with an short 3 second delay.
       */
      Serial.println("MELODY START");
      int notes[] = {523, 523, 784, 784, 880, 880, 784, 0, 698, 698, 659, 659, 587, 587, 523, 0, 784, 784, 698, 698, 659, 659, 587, 0, 784, 784, 698, 698, 659, 659, 587, 0, 523, 523, 784, 784, 880, 880, 784, 0, 698, 698, 659, 659, 587, 587, 523, 0};
      Melody melody = MelodyFactory.load("Mixed Pitch", 225, notes, 80, false);
      player.playAsync(melody);
      Serial.println("MELODY STOP");
     
      Serial.println("RECORDING START");
      startRecording(&client);
      Serial.println("RECORDING STOP");
      
      run_melody = false;
    } else 
    if (run_whitenoise) {
      /**
       * Creates a fixed whiteNoise-like sound and passes it to the MelodyPlayer.
       * Also records the playback with an short 3 second delay.
       */
      Serial.println("WHITENOISE_START");
      int notes[] = {1288, 1449, 66, 0, 0, 31, 0, 0, 199, 133, 546, 0, 5, 0, 0, 537, 320, 2389, 202, 0, 1232, 198, 909, 0, 218, 1024, 696, 128, 0, 445, 76, 720, 216, 1088, 0, 201, 666, 0, 0, 0, 1980, 0, 652, 619, 0, 0, 964, 0, 717, 0, 0, 1256, 1431, 0, 0, 0, 728, 160, 303, 0, 586, 1116, 0, 0, 0, 761, 0, 0, 0, 0, 0, 15, 1501, 420, 1333, 0, 0, 378, 0, 0, 160, 0, 464, 0, 0, 0, 0, 0, 0, 1250, 103, 0, 389, 0, 1240, 0, 439, 0, 0, 0, 1895, 697, 0, 0, 0, 0, 0, 721, 0, 0, 0, 0, 711, 126, 585, 1189, 1149, 0, 536, 0, 0, 1919, 0, 0, 170, 17, 26, 0, 1081, 889, 0, 314, 658, 1032, 392, 695, 0, 0, 0, 1019, 977, 146, 0, 307, 1663, 1354, 0, 0, 0, 0, 188, 24, 964, 1267, 834, 1319, 0, 0, 500, 2678, 356, 0, 242, 1425, 0, 803, 0, 1272, 785, 304, 2000, 0, 0, 1854, 0, 2198, 0, 0, 0, 130, 201, 0, 1081, 0, 0, 0, 1819, 0, 0, 0, 0, 640, 410, 1440, 0, 268, 1173, 903, 0, 1128, 0, 1803, 154, 0, 270, 849, 1741, 0, 0, 586, 0, 0, 835, 0, 1126, 0, 0, 282, 154, 1600, 525, 309, 586, 0, 77, 0, 519, 0, 0, 699, 915, 0, 2003, 0, 835, 951, 224, 172, 1797, 890, 445, 0, 0, 1162, 194, 0, 0, 0, 686, 387, 997, 0, 986, 0, 0, 1733, 74, 0, 0, 0, 1559, 1376, 716, 184, 1042, 0, 452, 401, 88, 1648, 1755, 1323, 0, 1835, 702, 0, 0, 1138, 1174, 855, 140, 35, 832, 0, 0, 0, 0, 332, 2263, 0, 477, 0, 300, 1353, 1241, 0, 0, 0, 0, 1246, 0, 703, 708, 397, 1084, 0, 0, 0, 926, 0, 0, 834, 0, 1770, 666, 0, 0, 1081, 0, 0, 6, 202, 15, 387, 0, 0, 1265, 645, 0, 1714, 0, 84, 668, 972, 111, 0, 585, 0, 474, 0, 381, 0, 940, 747, 728, 0, 434, 0, 214, 0, 0, 1977, 723, 0, 893, 0, 0, 0, 0, 240, 0, 0, 0, 364, 1769, 0, 0, 0, 653, 0, 0, 554, 0, 222, 0, 0, 0, 0, 0, 431, 547, 548, 479, 0, 0, 801, 12, 121, 0, 0, 0, 0, 0, 0, 85, 1165, 0, 94, 0, 671, 1863, 0, 0, 1423, 367, 114, 0, 0, 918, 1436, 641, 0, 0, 0, 0, 1122, 0, 0, 1319, 0, 1260, 0, 339, 679, 262, 1271, 15, 0, 0, 0, 0, 982, 825, 1391, 2727, 713, 501, 0, 0, 2195, 529, 0, 309, 0, 0, 0, 0, 768, 965, 0, 346, 0, 452, 763, 1534, 1560, 487, 0, 0, 0, 616, 565, 18, 1663, 648, 16, 0, 77, 0, 0, 347, 0, 0, 1220, 0, 1313, 0, 1516, 464, 0, 1238, 0, 0, 113, 155, 0, 0, 547, 1412, 1140, 1222, 1120, 0, 0, 187, 0, 770, 891, 0, 0, 0, 0, 0, 0, 0, 387, 0, 950, 313, 0, 0, 69, 0, 469, 806, 19, 0, 0, 572, 0, 1109, 0, 519, 0, 0, 0, 0, 574, 0, 0, 0, 64, 0, 675, 0, 1114, 0, 0, 1331, 0, 0, 75, 0, 0, 0, 0, 0, 0, 1612, 0, 971, 0, 544, 0, 0, 635, 0, 0, 0, 0, 573, 0, 0, 62, 0, 0, 0, 437, 0, 0, 0, 0, 0, 0, 0, 0, 174, 659, 597, 0, 1679, 854, 0, 0, 0, 0, 712, 1268, 0, 0, 0, 1361, 143, 1275, 827, 1560, 598, 0, 445, 2537, 0, 0, 2103, 408, 0, 0, 0, 703, 143, 0, 0, 0, 1067, 0, 1379, 0, 0, 0, 0, 0, 1025, 1209, 0, 1278, 95, 1588, 0, 0, 789, 623, 0, 22, 130, 311, 0, 0, 55, 259, 0, 0, 1343, 0, 0, 1594, 1132, 1037, 834, 569, 0, 30, 355, 633, 475, 0, 0, 0, 0, 0, 0, 0, 307, 0, 579, 0, 0, 890, 0, 0, 0, 1211, 30, 0, 149, 0, 904, 1169, 914, 343, 763, 812, 1165, 0, 345, 78, 159, 0, 0, 494, 199, 128, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2173, 863, 0, 0, 0, 0, 0, 0, 0, 822, 644, 1956, 0, 676, 0, 0, 0, 0, 0, 2737, 1305, 1820, 1194, 0, 413, 142, 434, 0, 0, 2104, 1192, 307, 0, 182, 0, 959, 164, 0, 0, 0, 131, 0, 962, 209, 0, 0, 1217, 1296, 688, 0, 0, 992, 36, 1275, 0, 796, 524, 0, 0, 0, 0, 0, 1589, 0, 789, 0, 0, 0, 407, 0, 530, 802, 0, 0, 0, 1080, 1768, 496, 0, 0, 0, 889, 0, 1479, 0, 0, 1317, 1783, 0, 792, 2530, 1168, 0, 281, 2376, 0, 913, 0, 1587, 0, 807, 911, 0, 0, 329, 0, 0, 0, 1342, 0, 0, 642, 1222, 0, 279, 492, 0, 0, 532, 0, 0, 855, 434, 141, 0, 0, 609, 484, 0, 0, 342, 184, 848, 0, 921, 1763, 949, 131, 906, 0, 0, 2059, 0, 0, 822, 0, 0, 0, 1682, 0, 0, 0, 769, 0, 498, 1580, 153, 0, 0, 82, 1320, 0, 0, 0, 657, 0, 311, 783, 0, 0, 618, 581, 1258, 0, 1230, 0, 0, 0, 0, 0, 1034, 0, 0, 768, 0, 792, 0, 0, 0, 0, 739, 1215, 1631, 0, 0, 0, 0, 1354, 1177, 0, 1831, 0, 559, 0, 0, 395, 0, 1193, 0, 105, 0, 135, 0, 820, 610, 69, 0, 1971, 0, 0, 772, 0, 0, 0, 0, 0, 194, 0, 0, 0, 1561, 0, 442, 286, 705, 0, 753, 124, 0, 312, 0, 949, 218, 0, 0, 0, 0, 0, 0, 1984, 458, 0, 0, 0, 0, 0, 0, 804, 0, 225, 1101, 0, 0, 0, 0, 949, 0, 1144, 411, 0, 287, 0, 0, 1422, 366, 1156, 0, 1046, 824, 0, 0, 100, 0, 0, 55, 0, 0, 11, 1459, 0, 2351, 0, 0, 1239, 0, 619, 395, 0, 0, 1546, 0, 278, 432, 1207, 0, 0, 0, 0, 613, 831, 0, 1515, 0, 690, 0, 833, 0, 1159, 862, 1869, 0, 0, 829, 317, 0, 0, 799, 0, 0, 1069, 0, 458, 490, 593, 1058, 653, 0, 0, 0, 0, 404, 1229, 782, 0, 201, 5, 0, 1299, 614, 361, 0, 0, 0, 1139, 0, 0, 0, 480, 0, 1714, 0, 0, 1420, 750, 695, 527, 0, 317, 536, 106, 0, 1377, 0, 0, 0, 0, 0, 0, 884, 0, 472, 0, 755, 0, 699, 0, 0, 0, 0, 0, 0, 0, 1136, 0, 0, 0, 0, 0, 232, 1042, 0, 0, 0, 1218, 0, 170, 768, 586, 0, 0, 0, 0, 0, 0, 842, 163, 0, 0, 473, 0, 506, 0, 1056, 0, 0, 1800, 1044, 1640, 0, 728, 1000, 914, 0, 1471, 415, 0, 2463, 126, 1256, 0, 0, 882, 821, 0, 246, 0, 0, 1092, 0, 742, 1065, 370, 1517, 363, 293, 1, 420, 352, 0, 0, 0, 2843, 1226, 0, 595, 0, 55, 1816, 82, 1282, 0, 458, 374, 0, 0, 1895, 0, 1204, 1719, 0, 1003, 366, 0, 585, 435, 0, 0, 0, 0, 0, 0, 0, 673, 1266, 0, 71, 0, 0, 2113, 289};

      Melody melody = MelodyFactory.load("whiteNoise", 10, notes, 1000, false);
      player.playAsync(melody);
      //player2.playAsync(melody);
      Serial.println("WHITENOISE STOP");
      
      Serial.println("RECORDING START");
      startRecording(&client);
      Serial.println("RECORDING STOP");
      
      run_whitenoise = false;
    } else 
    if (run_increase) {
      /**
       * Creates a fixed pitch increase and passes it to the MelodyPlayer.
       * Also records the playback with an short 3 second delay.
       */
      Serial.println("INCREASE_START");
      int _notes[] = {220, 245, 270, 295, 320, 345, 370, 395, 420, 445, 470, 495, 520, 545, 570, 595, 620, 645, 670, 695, 720, 745, 770, 795, 820, 845, 870, 895, 920, 945, 970, 995, 1020, 1045, 1070, 1095, 1120, 1145, 1170, 1195, 1220, 1245, 1270, 1295, 1320, 1345, 1370, 1395, 1420, 1445, 1470, 1495, 1520, 1545, 1570, 1595, 1620, 1645, 1670, 1695, 1720, 1745, 1770, 1795, 1820, 1845, 1870, 1895, 1920, 1945, 1970, 1995, 2020, 2045, 2070, 2095, 2120, 2145, 2170, 2195, 2220, 2245, 2270, 2295, 2320, 2345, 2370, 2395, 2420, 2445, 2470, 2495, 2520, 2545, 2570, 2595, 2620, 2645, 2670, 2695, 2720, 2745, 2770, 2795, 2820, 2845, 2870, 2895, 2920, 2945, 2970, 2995, 3020, 3045, 3070, 3095, 3120, 3145, 3170, 3195, 3220, 3245, 3270, 3295, 3320, 3345, 3370, 3395, 3420, 3445, 3470, 3495};      
      Melody increasing_figure = MelodyFactory.load("increase", 125, _notes, 90, false);
      player.playAsync(increasing_figure);
      Serial.println("INCREASE_STOP");

      Serial.println("RECORDING START");
      startRecording(&client);
      Serial.println("RECORDING STOP");
      
      run_increase = false;
    }
    
    // Clear the header variable
    header = "";
    // Close the connection
    client.stop();
    Serial.println("Client disconnected.");
    Serial.println("");
  }
}


/**
 * Method which uses an existing WiFiClient to establish an data downstream from the opening browser.
 * The datastream contains the ressources generated and recorded by the i2s_adc method.
 * 
 * @param client - The client which is currently connected to the hosted webserver
 */
void startRecording(WiFiClient* client) {
  setFilename(filename, sizeof(filename));
  client->println("Content-Type: application/octet-stream"); // octet-stream used to initiate a file download
  client->print("Content-Disposition: attachment; filename=\"");
  client->print(filename);
  client->println("\"");
  //client->println("Transfer-Encoding: chunked");
  client->println("Connection: close");
  client->println();

  byte header[headerSize];
  wavHeader(header, FLASH_RECORD_SIZE);
  client->write(header, headerSize);
  
  i2s_adc(client);
}


/**
 * Standard Method to configure the Inter-IC Sound interface.
 */
void i2sInit() {
  i2s_config_t i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate = I2S_SAMPLE_RATE,
    .bits_per_sample = i2s_bits_per_sample_t(I2S_SAMPLE_BITS),
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = i2s_comm_format_t(I2S_COMM_FORMAT_I2S | I2S_COMM_FORMAT_I2S_MSB),
    .intr_alloc_flags = 0,
    .dma_buf_count = 64,
    .dma_buf_len = 1024,
    .use_apll = 1
  };

  i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL);

  const i2s_pin_config_t pin_config = {
    .bck_io_num = I2s_SCK,
    .ws_io_num = I2S_WS,
    .data_out_num = -1,
    .data_in_num = I2S_SD
  };

  i2s_set_pin(I2S_PORT, &pin_config);
}


/**
 *  Records the input listend to by the microphone and sends it to the given client for the download process.
 *  Also prints updates on the recording progession to the serial monitor.
 *  
 *  @param wificlient: The currently established webclient used to download the sounddata.
 */
void i2s_adc(WiFiClient* wificlient) {
  int flash_wr_size = 0;
  size_t bytes_read;

  Serial.print("I2S_READ_LEN: ");
  Serial.println(I2S_READ_LEN);

  i2s_read(I2S_PORT, (void*) i2s_read_buff, I2S_READ_LEN, &bytes_read, portMAX_DELAY);
  i2s_read(I2S_PORT, (void*) i2s_read_buff, I2S_READ_LEN, &bytes_read, portMAX_DELAY);

  Serial.println(" *** Start recording ***");
  while (flash_wr_size < FLASH_RECORD_SIZE) {
    i2s_read(I2S_PORT, (void*) i2s_read_buff, I2S_READ_LEN, &bytes_read, portMAX_DELAY);

    Serial.print("Bytes read: ");
    Serial.println(bytes_read);

    i2s_adc_data_scale(flash_write_buff, (uint8_t*)i2s_read_buff, I2S_READ_LEN);
    Serial.println("i2s_adc_data_scale returned.");
    wificlient->write((const byte*) flash_write_buff, I2S_READ_LEN); // sends each read byte of data to the WiFiClient
    flash_wr_size += I2S_READ_LEN;
    ets_printf("Sound recording %u%%\n", flash_wr_size * 100 / FLASH_RECORD_SIZE);
    ets_printf("Never used Stack size: %u\n", uxTaskGetStackHighWaterMark(NULL));
    bufferZero();
  }
}


/**
 *  Upscales the recorded sound data to increase its volume.
 *  The increase is determined by the parameter 'scale_factor'.
 */
void i2s_adc_data_scale(uint8_t* d_buff, uint8_t* s_buff, uint32_t len) {
  uint32_t j = 0;
  uint32_t dac_value = 0;
  uint32_t scale_factor =  4096;

  for (int i = 0; i < len; i += 2) {
    dac_value = ((((uint16_t) (s_buff[i + 1] & 0xf) << 8) | ((s_buff[i + 0]))));
    d_buff[j++] = 0;
    d_buff[j++] = dac_value * 256 / scale_factor;
  }
}


/**
 * Initializes the read and write buffers only once to prevent memory fragmentation.
 */
void buffersInit(){
  Serial.println("Initializing i2s_read_buff and flash_write_buff ...");
  i2s_read_buff = (char*) calloc(I2S_READ_LEN, sizeof(char));
  flash_write_buff = (uint8_t*) calloc(I2S_READ_LEN, sizeof(char));

  if(i2s_read_buff == NULL) {
    Serial.println("!!! Calloc1 failed: char* i2s_read_buff = (char*)calloc(I2S_READ_LEN, sizeof(char));");
    for(;;); // stop execution forever
  } else {
    Serial.println(ets_printf("    i2s_read_buff: %p", i2s_read_buff));
  }

  if(flash_write_buff == NULL) {
    Serial.println("!!! Calloc2 failed: uint8_t* flash_write_buff = (uint8_t*)calloc(I2S_READ_LEN, sizeof(char));");
    for(;;); // stop execution forever
  } else {
    Serial.println(ets_printf("    flash_write_buff: %p", flash_write_buff));
  }
  
  Serial.println("initialization done.");
}


/**
 * Resets both buffers to zero (0x00).
 */
void bufferZero() {
  memset(i2s_read_buff, 0x00, I2S_READ_LEN);
  memset(flash_write_buff, 0x00, I2S_READ_LEN);
}


/**
 * https://playground.arduino.cc/Code/AvailableMemory/
 */
int freeRam () {
  return ESP.getFreeHeap();
}


/**
 * Prints the total size of not allocated RAM of the controller to the serial monitor.
 */
void printFreeRAM() {
  return; // disable printing of free ram 
  
  Serial.print("Free RAM: ");
  Serial.println(freeRam());
}


/**
 * Creates the header for a .wav-file.
 * 
 * @param header:
 * @param wavSize:
 */
void wavHeader(byte* header, int wavSize) {
  header[0] = 'R';
  header[1] = 'I';
  header[2] = 'F';
  header[3] = 'F';
  unsigned int fileSize =  wavSize + headerSize - 8;
  header[4] = (byte)(fileSize & 0xFF);
  header[5] = (byte)((fileSize >> 8) & 0xFF);
  header[6] = (byte)((fileSize >> 16) & 0xFF);
  header[7] = (byte)((fileSize >> 24) & 0xFF);
  header[8] = 'W';
  header[9] = 'A';
  header[10] = 'V';
  header[11] = 'E';
  header[12] = 'f';
  header[13] = 'm';
  header[14] = 't';
  header[15] = ' ';
  header[16] = 0x10;  // linear PCM
  header[17] = 0x00;
  header[18] = 0x00;
  header[19] = 0x00;
  header[20] = 0x01;  // linear PCM
  header[21] = 0x00;
  header[22] = 0x01;  // monoral
  header[23] = 0x00;
  header[24] = 0x80;  // sampling rate 44100
  header[25] = 0x3E;
  header[26] = 0x00;
  header[27] = 0x00;
  header[28] = 0x00;  // Byte/sec = 44100x2x1 = 88200
  header[29] = 0x7D;
  header[30] = 0x00; // 0x00 zum gucken ob dann header nicht deformed, ansonsten war 0x01
  header[31] = 0x00;
  header[32] = 0x02;  // 16bit monoral
  header[33] = 0x00;
  header[34] = 0x10;  // 16bit
  header[35] = 0x00;
  header[36] = 'd';
  header[37] = 'a';
  header[38] = 't';
  header[39] = 'a';
  header[40] = (byte)(wavSize & 0xFF);
  header[41] = (byte)((wavSize >> 8) & 0xFF);
  header[42] = (byte)((wavSize >> 16) & 0xFF);
  header[43] = (byte)((wavSize >> 24) & 0xFF);
}
