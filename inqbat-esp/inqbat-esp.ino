#include <WiFi.h>
#include <WiFiClient.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <Update.h>
#include <DHT.h>
#include <FirebaseESP32.h>
#include <PID_v1.h>

const char* host = "main";

// 와이파이 설정.
const char* ssid = "inqbat";
const char* password = "inqbat123!";

// 사실 PID 제어 사용안해서 빼도 됨ㅋㅋ
// PID 제어 변수 초기화
double setpoint = 25.0;
double input, output;
double integral, previousError;
double Kp = 2.0, Ki = 5.0, Kd = 1.0; // P, I, D 값

PID pid(&input, &output, &setpoint, Kp, Ki, Kd, DIRECT);


#define DHTPIN 32                             // DHT22 센서의 데이터 핀을 ESP-WROOM-32의 2번 핀에 연결합니다.
#define DHTTYPE DHT22                         // DHT11 대신 DHT22 센서를 사용합니다.

#define FIREBASE_HOST ""                      // Firebase 호스트 주소를 여기에 입력하세요.
#define FIREBASE_AUTH ""                      // Firebase 인증 토큰을 여기에 입력하세요.
#define FIREBASE_DATABASE_PATH "/inqbat2"     // Firebase Realtime Database 경로를 설정합니다.

// 출력 핀 값 설정.
#define UV_LED_PIN 16
#define LED_PIN 17
#define COOL_FAN_PIN 27
#define HIT_FAN_PIN 26
#define AIR_FAN_PIN 25
#define CAM_PIN 33

// DHT 객체 생성
DHT dht(DHTPIN, DHTTYPE);

// Firebase 객체 생성
FirebaseData firebaseData;

// 웹서버 실행 (http://main.local/)
// 웹 업로드(.bin) 할려고.. 계속 플래시 버튼 누르면 손 아파서.
WebServer server(80);

/*
 * Login page
 */

const char* loginIndex =
 "<form name='loginForm'>"
    "<table width='20%' bgcolor='A09F9F' align='center'>"
        "<tr>"
            "<td colspan=2>"
                "<center><font size=4><b>ESP32 Login Page</b></font></center>"
                "<br>"
            "</td>"
            "<br>"
            "<br>"
        "</tr>"
        "<tr>"
             "<td>Username:</td>"
             "<td><input type='text' size=25 name='userid'><br></td>"
        "</tr>"
        "<br>"
        "<br>"
        "<tr>"
            "<td>Password:</td>"
            "<td><input type='Password' size=25 name='pwd'><br></td>"
            "<br>"
            "<br>"
        "</tr>"
        "<tr>"
            "<td><input type='submit' onclick='check(this.form)' value='Login'></td>"
        "</tr>"
    "</table>"
"</form>"
"<script>"
    "function check(form)"
    "{"
    "if(form.userid.value=='inqbat' && form.pwd.value=='inqbat123!')"
    "{"
    "window.open('/serverIndex')"
    "}"
    "else"
    "{"
    " alert('Error Password or Username')/*displays error message*/"
    "}"
    "}"
"</script>";

/*
 * Server Index Page
 */

const char* serverIndex =
"<script src='https://ajax.googleapis.com/ajax/libs/jquery/3.2.1/jquery.min.js'></script>"
"<form method='POST' action='#' enctype='multipart/form-data' id='upload_form'>"
   "<input type='file' name='update'>"
        "<input type='submit' value='Update'>"
    "</form>"
 "<div id='prg'>progress: 0%</div>"
 "<script>"
  "$('form').submit(function(e){"
  "e.preventDefault();"
  "var form = $('#upload_form')[0];"
  "var data = new FormData(form);"
  " $.ajax({"
  "url: '/update',"
  "type: 'POST',"
  "data: data,"
  "contentType: false,"
  "processData:false,"
  "xhr: function() {"
  "var xhr = new window.XMLHttpRequest();"
  "xhr.upload.addEventListener('progress', function(evt) {"
  "if (evt.lengthComputable) {"
  "var per = evt.loaded / evt.total;"
  "$('#prg').html('progress: ' + Math.round(per*100) + '%');"
  "}"
  "}, false);"
  "return xhr;"
  "},"
  "success:function(d, s) {"
  "console.log('success!')"
 "},"
 "error: function (a, b, c) {"
 "}"
 "});"
 "});"
 "</script>";

/*
 * setup function
 */
void setup(void) {
  Serial.begin(115200);

  // Connect to WiFi network
  WiFi.begin(ssid, password);
  Serial.println("");

  // Wait for connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  /*use mdns for host name resolution*/
  if (!MDNS.begin(host)) { //http://main.local
    Serial.println("Error setting up MDNS responder!");
    while (1) {
      delay(1000);
    }
  }
  Serial.println("mDNS responder started");
  /*return index page which is stored in serverIndex */
  server.on("/", HTTP_GET, []() {
    server.sendHeader("Connection", "close");
    server.send(200, "text/html", loginIndex);
  });
  server.on("/serverIndex", HTTP_GET, []() {
    server.sendHeader("Connection", "close");
    server.send(200, "text/html", serverIndex);
  });
  /*handling uploading firmware file */
  server.on("/update", HTTP_POST, []() {
    server.sendHeader("Connection", "close");
    server.send(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK");
    ESP.restart();
  }, []() {
    HTTPUpload& upload = server.upload();
    if (upload.status == UPLOAD_FILE_START) {
      Serial.printf("Update: %s\n", upload.filename.c_str());
      if (!Update.begin(UPDATE_SIZE_UNKNOWN)) { //start with max available size
        Update.printError(Serial);
      }
    } else if (upload.status == UPLOAD_FILE_WRITE) {
      /* flashing firmware to ESP*/
      if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
        Update.printError(Serial);
      }
    } else if (upload.status == UPLOAD_FILE_END) {
      if (Update.end(true)) { //true to set the size to the current progress
        Serial.printf("Update Success: %u\nRebooting...\n", upload.totalSize);
      } else {
        Update.printError(Serial);
      }
    }
  });
  server.begin();

  Firebase.begin(FIREBASE_HOST, FIREBASE_AUTH);      // Firebase 초기화합니다.
  dht.begin();                                      // DHT 센서를 초기화합니다.
  pinMode(LED_PIN, OUTPUT);                          // LED 핀을 출력으로 설정합니다.
  pinMode(UV_LED_PIN, OUTPUT);
  pinMode(COOL_FAN_PIN, OUTPUT);
  pinMode(HIT_FAN_PIN, OUTPUT);
  pinMode(AIR_FAN_PIN, OUTPUT);
  pinMode(CAM_PIN, OUTPUT);
}

void loop(void) {
  server.handleClient();

  delay(100);                                      // 2초 동안 대기합니다.

  float humidity = dht.readHumidity();              // 습도를 읽어옵니다.
  float temperature = dht.readTemperature();        // 온도를 읽어옵니다.

  if (isnan(humidity) || isnan(temperature)) {      // 습도 또는 온도가 유효하지 않은 값일 경우
    // Serial.println("Failed to read from DHT sensor!");
    // return;            // 메이커집이 주석으로 변경
  }

  Firebase.setFloat(firebaseData, FIREBASE_DATABASE_PATH + String("/humi"), humidity);            // Firebase에 습도를 업데이트합니다.
  Firebase.setFloat(firebaseData, FIREBASE_DATABASE_PATH + String("/temp"), temperature);      // Firebase에 온도를 업데이트합니다.
  // Firebase.setString(firebaseData, FIREBASE_DATABASE_PATH + String("/camUrl"), Wifi().LocalIP());

  input = temperature;

  Serial.println("==============");
  // UV LED
  if (Firebase.getString(firebaseData, FIREBASE_DATABASE_PATH + String("/isUvLed"))) {
    if (firebaseData.dataType() == "string") {
      String value = firebaseData.stringData();
      if (value == "true") {
        digitalWrite(UV_LED_PIN, LOW);  // LED 핀을 LOW로 설정합니다.
        Serial.println("UV LED on");
      } else if (value == "false") {
        digitalWrite(UV_LED_PIN, HIGH);   // LED 핀을 HIGH로 설정합니다.
        Serial.println("UV LED off");
      }
    }
  }

  // LED
  if (Firebase.getString(firebaseData, FIREBASE_DATABASE_PATH + String("/isLed"))) {
    if (firebaseData.dataType() == "string") {
      String value = firebaseData.stringData();
      if (value == "true") {
        digitalWrite(LED_PIN, LOW);  // LED 핀을 LOW로 설정합니다.
        Serial.println("LED on");
      } else if (value == "false") {
        digitalWrite(LED_PIN, HIGH);   // LED 핀을 HIGH로 설정합니다.
        Serial.println("LED off");
      }
    }
  }

  // Cam
  if (Firebase.getString(firebaseData, FIREBASE_DATABASE_PATH + String("/isCam"))) {
    if (firebaseData.dataType() == "string") {
      String value = firebaseData.stringData();
      if (value == "true") {
        digitalWrite(CAM_PIN, LOW);  // LED 핀을 LOW로 설정합니다.
        Serial.println("CAM on");
      } else if (value == "false") {
        digitalWrite(CAM_PIN, HIGH);   // LED 핀을 HIGH로 설정합니다.
        Serial.println("CAM off");
      }
    }
  }

  // AIR_FAN
  if (Firebase.getString(firebaseData, FIREBASE_DATABASE_PATH + String("/isOutFan"))) {
    if (firebaseData.dataType() == "string") {
      String value = firebaseData.stringData();
      if (value == "true") {
        int mappedValue = map(temperature, 17, 33, 0, 255);  // 여기서 팬 속도 제어함.
        analogWrite(AIR_FAN_PIN, mappedValue);
      } else if (value == "false") {
        analogWrite(AIR_FAN_PIN, 0);   // LED 핀을 HIGH로 설정합니다.
        Serial.println("AIR off");
      }
    }
  }

   // COOL_FAN
  if (Firebase.getString(firebaseData, FIREBASE_DATABASE_PATH + String("/isCoolFan"))) {
    if (firebaseData.dataType() == "string") {
      String value = firebaseData.stringData();
      if (value == "true") {
        int mappedValue = map(temperature, 17, 33, 0, 255);  // 여기서 팬 속도 제어함.
        analogWrite(COOL_FAN_PIN, mappedValue);
        Serial.println("cool on");
      } else if (value == "false") {
        analogWrite(COOL_FAN_PIN, 0);   // LED 핀을 HIGH로 설정합니다.
        Serial.println("cool off");
      }
    }
  }

  // HIT_FAN
  if (Firebase.getString(firebaseData, FIREBASE_DATABASE_PATH + String("/isHitFan"))) {
    if (firebaseData.dataType() == "string") {
      String value = firebaseData.stringData();
      if (value == "false") {
        digitalWrite(HIT_FAN_PIN, LOW);   // LED 핀을 HIGH로 설정합니다.
        Serial.println("hit on");
      } else if (value == "true") {
        digitalWrite(HIT_FAN_PIN, HIGH);   // LED 핀을 HIGH로 설정합니다.
        Serial.println("hit off");
      }
    }
  }
  delay(100);
}
