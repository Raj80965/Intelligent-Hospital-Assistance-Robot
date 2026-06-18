/*
 * ======================================================================================
 *  HUMAN FOLLOWING ROBOT - OLED + AUTO/MANUAL CONTROL (FIXED)
 *  ======================================================================================
 */

#include <WiFi.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <ESP32Servo.h>
#include <Wire.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_GFX.h>

// ==================== OLED SETUP ====================
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_ADDRESS 0x3C
#define I2C_SDA 26
#define I2C_SCL 27

// Use Wire instead of TwoWire for better compatibility
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
bool oledWorking = false;

// ==================== PHONE HOTSPOT SETTINGS ====================
const char* ssid = "Redmi 12 5G";        
const char* password = "12guruji21";

// ==================== Web Server ====================
WebServer server(80);
WebSocketsServer webSocket = WebSocketsServer(81);

// ==================== PIN DEFINITIONS ====================
#define TRIG_PIN         32
#define ECHO_PIN         34
#define SERVO_PIN        14

// Motor Driver 1 (LEFT SIDE)
#define L_ENA            15
#define L_IN1            2
#define L_IN2            4
#define L_ENB            18
#define L_IN3            19
#define L_IN4            21

// Motor Driver 2 (RIGHT SIDE)
#define R_ENA            22
#define R_IN1            23
#define R_IN2            5
#define R_ENB            12
#define R_IN3            13
#define R_IN4            33

// ==================== SERVO POSITIONS ====================
#define SERVO_LEFT       0
#define SERVO_CENTER     90
#define SERVO_RIGHT      180

// ==================== DISTANCE SETTINGS ====================
#define ZONE_CRITICAL    25
#define ZONE_TOO_CLOSE   35
#define ZONE_MAINTAIN    38
#define ZONE_IDEAL       40
#define ZONE_FOLLOW      50
#define ZONE_FAR         65
#define ZONE_VERY_FAR    80
#define ZONE_LOST        100

// ==================== SPEED SETTINGS ====================
#define SPEED_BACK_FAST   220
#define SPEED_BACK_NORMAL 160
#define SPEED_MAINTAIN    90
#define SPEED_IDEAL       130
#define SPEED_NORMAL      160
#define SPEED_FAST        200
#define SPEED_VERY_FAST   230
#define SPEED_MAX         245
#define SPEED_TURN        150
#define MANUAL_SPEED      180
#define MANUAL_TURN_SPEED 160

// ==================== TIMING ====================
#define LOOP_DELAY        25
#define TURN_DURATION     250
#define BACK_DURATION     180
#define SCAN_DELAY        250
#define LOST_TIMEOUT      15
#define OLED_UPDATE_DELAY 200

// ==================== GLOBAL VARIABLES ====================
Servo myServo;
int currentDistance = 40;
int currentSpeed = 0;
String currentAction = "READY";
String currentMode = "AUTO";
bool isScanning = false;
int lostCounter = 0;
unsigned long lastPrint = 0;
unsigned long lastSensorSend = 0;
unsigned long lastOLEDupdate = 0;

String manualCommand = "STOP";
int smoothSpeed = 0;
#define ACCEL_STEP 5

int distanceHistory[5];
int histIndex = 0;

float maxDist = 0;
float minDist = 200;
long totalReadings = 0;
float avgDist = 0;

// ==================== FUNCTION DECLARATIONS ====================
void initOLED();
void printHeader();
void initHardware();
void connectToPhoneHotspot();
void initWebServer();
void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length);
String getDashboardHTML();
int getDistance();
int getFilteredDistance();
void setMotorSpeed(int speed);
void smoothMove(int target);
void forward(int speed);
void backward(int speed);
void turnLeft(int speed);
void turnRight(int speed);
void stopRobot();
void scanForHuman();
void sendSensorData();
void executeManualCommand();
void updateOLED();
void autoMode();
void manualMode();
void followHuman();

// ==================== SETUP ====================
void setup() {
  Serial.begin(115200);
  delay(1000);
  
  initOLED();
  printHeader();
  initHardware();
  connectToPhoneHotspot();
  initWebServer();
  
  Serial.println("\n✅ ROBOT READY!");
  Serial.print("📱 Dashboard: http://");
  Serial.println(WiFi.localIP());
  Serial.println("📺 OLED Display: " + String(oledWorking ? "WORKING" : "NOT DETECTED"));
  Serial.println("🎮 MODES: AUTO | MANUAL\n");
  
  delay(1000);
  myServo.write(SERVO_CENTER);
}

// ==================== OLED INITIALIZATION (FIXED) ====================
void initOLED() {
  Serial.println("\n📺 Initializing OLED...");
  
  // Initialize I2C
  Wire.begin(I2C_SDA, I2C_SCL);
  Wire.setClock(400000);
  Serial.println("   I2C Started on SDA=26, SCL=27");
  
  // Scan I2C devices
  Serial.println("   Scanning I2C devices...");
  for (uint8_t addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) {
      Serial.print("   ✅ Device found at 0x");
      Serial.println(addr, HEX);
      if (addr == OLED_ADDRESS) {
        Serial.println("   ✅ OLED Address 0x3C found!");
      }
    }
  }
  
  // Initialize display
  if (display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS)) {
    oledWorking = true;
    display.clearDisplay();
    display.display();
    Serial.println("   ✅ OLED Connected Successfully!");
    
    // Test display
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(10, 20);
    display.println("ROBOT READY");
    display.setCursor(10, 35);
    display.print("IP: ");
    display.println(WiFi.localIP());
    display.setCursor(10, 50);
    display.println("MODE: AUTO");
    display.display();
    
  } else {
    oledWorking = false;
    Serial.println("   ❌ OLED Failed!");
    Serial.println("   CHECK:");
    Serial.println("   - SDA -> GPIO26");
    Serial.println("   - SCL -> GPIO27");
    Serial.println("   - VCC -> 3.3V or 5V");
    Serial.println("   - GND -> GND");
  }
}

// ==================== HEADER ====================
void printHeader() {
  Serial.println("\n");
  Serial.println("╔══════════════════════════════════════════════════════════════════╗");
  Serial.println("║     HUMAN FOLLOWING ROBOT - OLED + AUTO/MANUAL + DASHBOARD       ║");
  Serial.println("╠══════════════════════════════════════════════════════════════════╣");
  Serial.println("║  📺 OLED: Distance | Speed | Mode | Action                        ║");
  Serial.println("║  🎮 AUTO MODE: Robot follows human automatically                  ║");
  Serial.println("║  🎮 MANUAL MODE: Control robot from phone dashboard               ║");
  Serial.println("╚══════════════════════════════════════════════════════════════════╝");
  Serial.println();
}

// ==================== HARDWARE INIT ====================
void initHardware() {
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  
  myServo.attach(SERVO_PIN);
  myServo.write(SERVO_CENTER);
  
  pinMode(L_IN1, OUTPUT); pinMode(L_IN2, OUTPUT);
  pinMode(L_IN3, OUTPUT); pinMode(L_IN4, OUTPUT);
  pinMode(R_IN1, OUTPUT); pinMode(R_IN2, OUTPUT);
  pinMode(R_IN3, OUTPUT); pinMode(R_IN4, OUTPUT);
  
  ledcAttach(L_ENA, 5000, 8);
  ledcAttach(L_ENB, 5000, 8);
  ledcAttach(R_ENA, 5000, 8);
  ledcAttach(R_ENB, 5000, 8);
  
  for (int i = 0; i < 5; i++) distanceHistory[i] = 40;
  
  stopRobot();
  Serial.println("✅ Hardware Initialized");
}

// ==================== CONNECT TO HOTSPOT ====================
void connectToPhoneHotspot() {
  Serial.print("\n📱 Connecting to: ");
  Serial.println(ssid);
  
  WiFi.begin(ssid, password);
  int attempts = 0;
  
  while (WiFi.status() != WL_CONNECTED && attempts < 30) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n✅ Connected!");
    Serial.print("📱 IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\n❌ Connection Failed!");
  }
}

// ==================== WEB SERVER ====================
void initWebServer() {
  server.on("/", []() { server.send(200, "text/html", getDashboardHTML()); });
  server.begin();
  webSocket.begin();
  webSocket.onEvent(webSocketEvent);
  Serial.println("✅ Web Server Started");
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
  if (type == WStype_TEXT) {
    String cmd = String((char*)payload);
    
    if (cmd == "auto") {
      currentMode = "AUTO";
      manualCommand = "STOP";
      stopRobot();
    }
    else if (cmd == "manual") {
      currentMode = "MANUAL";
      stopRobot();
    }
    else if (cmd == "forward") manualCommand = "FORWARD";
    else if (cmd == "backward") manualCommand = "BACKWARD";
    else if (cmd == "left") manualCommand = "LEFT";
    else if (cmd == "right") manualCommand = "RIGHT";
    else if (cmd == "stop") manualCommand = "STOP";
    else if (cmd == "servo_left") {
      myServo.write(SERVO_LEFT);
      delay(300);
      myServo.write(SERVO_CENTER);
    }
    else if (cmd == "servo_right") {
      myServo.write(SERVO_RIGHT);
      delay(300);
      myServo.write(SERVO_CENTER);
    }
    else if (cmd == "servo_center") myServo.write(SERVO_CENTER);
    else if (cmd == "servo_wave") {
      for (int i = 0; i < 3; i++) {
        myServo.write(SERVO_LEFT);
        delay(150);
        myServo.write(SERVO_RIGHT);
        delay(150);
      }
      myServo.write(SERVO_CENTER);
    }
  }
}

// ==================== HTML DASHBOARD ====================
String getDashboardHTML() {
  return "<!DOCTYPE html><html><head><meta name='viewport' content='width=device-width,initial-scale=1'><title>Robot Control</title><meta charset='UTF-8'><script src='https://cdn.jsdelivr.net/npm/chart.js'></script><style>*{margin:0;padding:0;box-sizing:border-box}body{font-family:'Segoe UI',sans-serif;background:linear-gradient(135deg,#667eea,#764ba2);min-height:100vh;padding:20px}.container{max-width:600px;margin:0 auto}h1{text-align:center;color:#fff;font-size:1.5em;margin-bottom:20px}.card{background:#fff;border-radius:20px;padding:20px;margin-bottom:20px;box-shadow:0 10px 30px rgba(0,0,0,0.2)}.distance-card{text-align:center;background:linear-gradient(135deg,#667eea,#764ba2)}.distance-value{font-size:80px;font-weight:bold;color:#fff;margin:10px 0}.status{font-size:20px;padding:12px;border-radius:12px;text-align:center;font-weight:bold;margin-top:15px}.status.auto{background:#2196F3;color:#fff}.status.manual{background:#FF9800;color:#fff}.mode-buttons{display:flex;gap:15px;justify-content:center;margin-bottom:20px}.mode-btn{background:#fff;color:#667eea;border:none;padding:12px 25px;border-radius:30px;font-size:18px;font-weight:bold;cursor:pointer}.mode-btn.active{background:#4CAF50;color:#fff}.control-panel{display:grid;grid-template-columns:repeat(3,1fr);gap:15px;max-width:300px;margin:0 auto}.ctrl-btn{background:#667eea;color:#fff;border:none;padding:20px;border-radius:20px;font-size:24px;cursor:pointer}.ctrl-btn:active{background:#764ba2}.ctrl-stop{background:#f44336}.servo-buttons{display:flex;gap:10px;justify-content:center;margin-top:15px;flex-wrap:wrap}.servo-btn{background:#9C27B0;color:#fff;border:none;padding:10px 20px;border-radius:20px;font-size:14px;cursor:pointer}.stats{display:grid;grid-template-columns:repeat(3,1fr);gap:15px;text-align:center;margin-top:15px}.stat-box{background:#f5f5f5;padding:10px;border-radius:12px}.stat-value{font-size:20px;font-weight:bold;color:#667eea}.stat-label{font-size:11px;color:#666}.speed-bar{height:25px;background:#667eea;border-radius:12px;margin-top:10px;transition:width 0.3s;display:flex;align-items:center;justify-content:center;color:#fff;font-size:11px}.chart-container{margin-top:15px}canvas{max-height:180px;width:100%}</style></head><body><div class='container'><h1>🤖 Robot Controller</h1><div class='card distance-card'><div class='distance-value' id='distance'>-- cm</div><div id='status' class='status auto'>AUTO MODE</div></div><div class='mode-buttons'><button class='mode-btn' id='autoBtn' onclick='setMode(\"auto\")'>🤖 AUTO</button><button class='mode-btn' id='manualBtn' onclick='setMode(\"manual\")'>🎮 MANUAL</button></div><div class='card' id='manualControls'><div class='control-panel'><button class='ctrl-btn' onclick='sendCmd(\"forward\")'>⬆️</button><button class='ctrl-btn' onclick='sendCmd(\"left\")'>⬅️</button><button class='ctrl-btn' onclick='sendCmd(\"stop\")'>🛑</button><button class='ctrl-btn' onclick='sendCmd(\"right\")'>➡️</button><button class='ctrl-btn' onclick='sendCmd(\"backward\")'>⬇️</button></div></div><div class='card'><div class='servo-buttons'><button class='servo-btn' onclick='sendCmd(\"servo_left\")'>⬅️ Look Left</button><button class='servo-btn' onclick='sendCmd(\"servo_center\")'>🎯 Center</button><button class='servo-btn' onclick='sendCmd(\"servo_right\")'>➡️ Look Right</button><button class='servo-btn' onclick='sendCmd(\"servo_wave\")'>👋 Wave</button></div></div><div class='card'><div class='stats'><div class='stat-box'><div class='stat-value' id='maxDist'>--</div><div class='stat-label'>📈 Max</div></div><div class='stat-box'><div class='stat-value' id='minDist'>--</div><div class='stat-label'>📉 Min</div></div><div class='stat-box'><div class='stat-value' id='avgDist'>--</div><div class='stat-label'>📊 Avg</div></div></div><div><div>⚡ Speed: <strong id='speed'>0</strong></div><div class='speed-bar' id='speedBar' style='width:0%'>0%</div><div>🎯 Action: <strong id='action'>STOP</strong></div></div></div><div class='card'><div class='chart-container'><canvas id='distChart'></canvas></div></div><div id='connStatus' style='text-align:center;font-size:12px;color:#4CAF50'>🟢 Connected</div></div><script>let distData=[],timeLabels=[];let maxD=0,minD=200,sumD=0,count=0;const ctx=document.getElementById('distChart').getContext('2d');const chart=new Chart(ctx,{type:'line',data:{labels:timeLabels,datasets:[{label:'Distance (cm)',data:distData,borderColor:'#667eea',borderWidth:3,fill:true,tension:0.4}]},options:{responsive:true,scales:{y:{min:0,max:120}}}});let ws=new WebSocket('ws://'+location.hostname+':81');ws.onmessage=function(e){try{let d=JSON.parse(e.data);document.getElementById('distance').innerHTML=d.dist+' cm';document.getElementById('speed').innerHTML=d.speed;document.getElementById('action').innerHTML=d.action;let spdPercent=(d.speed/255)*100;document.getElementById('speedBar').style.width=spdPercent+'%';document.getElementById('speedBar').innerHTML=Math.round(spdPercent)+'%';let sDiv=document.getElementById('status');if(d.mode=='AUTO'){sDiv.className='status auto';sDiv.innerHTML='🤖 AUTO MODE - Following'}else{sDiv.className='status manual';sDiv.innerHTML='🎮 MANUAL MODE - You Control'}let dist=d.dist;sumD+=dist;count++;if(dist>maxD)maxD=dist;if(dist<minD)minD=dist;document.getElementById('maxDist').innerHTML=Math.round(maxD);document.getElementById('minDist').innerHTML=Math.round(minD);document.getElementById('avgDist').innerHTML=(sumD/count).toFixed(1);distData.push(dist);timeLabels.push(distData.length);if(distData.length>30){distData.shift();timeLabels.shift()}chart.update();if(d.mode=='AUTO'){document.getElementById('autoBtn').classList.add('active');document.getElementById('manualBtn').classList.remove('active');}else{document.getElementById('manualBtn').classList.add('active');document.getElementById('autoBtn').classList.remove('active');}}catch(e){}};ws.onopen=function(){document.getElementById('connStatus').innerHTML='🟢 Connected to Robot'};ws.onclose=function(){document.getElementById('connStatus').innerHTML='🔴 Disconnected'};function setMode(m){sendCmd(m)}function sendCmd(c){if(ws.readyState===WebSocket.OPEN)ws.send(c)}</script></body></html>";
}

// ==================== DISTANCE SENSOR ====================
int getDistance() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  
  long duration = pulseIn(ECHO_PIN, HIGH, 35000);
  if (duration == 0) return currentDistance;
  
  int dist = duration * 0.034 / 2;
  return constrain(dist, 0, 400);
}

int getFilteredDistance() {
  int raw = getDistance();
  if (raw > 400 || raw < 0) raw = currentDistance;
  
  distanceHistory[histIndex] = raw;
  histIndex = (histIndex + 1) % 5;
  
  long sum = 0;
  for (int i = 0; i < 5; i++) sum += distanceHistory[i];
  return sum / 5;
}

// ==================== MOTOR CONTROL ====================
void setMotorSpeed(int speed) {
  speed = constrain(speed, 0, 255);
  currentSpeed = speed;
  ledcWrite(L_ENA, speed);
  ledcWrite(L_ENB, speed);
  ledcWrite(R_ENA, speed);
  ledcWrite(R_ENB, speed);
}

void smoothMove(int target) {
  target = constrain(target, 0, 255);
  if (target > smoothSpeed) {
    smoothSpeed += ACCEL_STEP;
    if (smoothSpeed > target) smoothSpeed = target;
  } else if (target < smoothSpeed) {
    smoothSpeed -= ACCEL_STEP;
    if (smoothSpeed < target) smoothSpeed = target;
  }
  setMotorSpeed(smoothSpeed);
}

void forward(int speed) {
  digitalWrite(L_IN1, HIGH); digitalWrite(L_IN2, LOW);
  digitalWrite(L_IN3, HIGH); digitalWrite(L_IN4, LOW);
  digitalWrite(R_IN1, HIGH); digitalWrite(R_IN2, LOW);
  digitalWrite(R_IN3, HIGH); digitalWrite(R_IN4, LOW);
  smoothMove(speed);
  currentAction = "FORWARD";
}

void backward(int speed) {
  digitalWrite(L_IN1, LOW); digitalWrite(L_IN2, HIGH);
  digitalWrite(L_IN3, LOW); digitalWrite(L_IN4, HIGH);
  digitalWrite(R_IN1, LOW); digitalWrite(R_IN2, HIGH);
  digitalWrite(R_IN3, LOW); digitalWrite(R_IN4, HIGH);
  smoothMove(speed);
  currentAction = "BACKWARD";
}

void turnLeft(int speed) {
  digitalWrite(L_IN1, LOW); digitalWrite(L_IN2, HIGH);
  digitalWrite(L_IN3, LOW); digitalWrite(L_IN4, HIGH);
  digitalWrite(R_IN1, HIGH); digitalWrite(R_IN2, LOW);
  digitalWrite(R_IN3, HIGH); digitalWrite(R_IN4, LOW);
  smoothMove(speed);
  currentAction = "TURN LEFT";
}

void turnRight(int speed) {
  digitalWrite(L_IN1, HIGH); digitalWrite(L_IN2, LOW);
  digitalWrite(L_IN3, HIGH); digitalWrite(L_IN4, LOW);
  digitalWrite(R_IN1, LOW); digitalWrite(R_IN2, HIGH);
  digitalWrite(R_IN3, LOW); digitalWrite(R_IN4, HIGH);
  smoothMove(speed);
  currentAction = "TURN RIGHT";
}

void stopRobot() {
  digitalWrite(L_IN1, LOW); digitalWrite(L_IN2, LOW);
  digitalWrite(L_IN3, LOW); digitalWrite(L_IN4, LOW);
  digitalWrite(R_IN1, LOW); digitalWrite(R_IN2, LOW);
  digitalWrite(R_IN3, LOW); digitalWrite(R_IN4, LOW);
  smoothMove(0);
  currentAction = "STOP";
}

// ==================== EXECUTE MANUAL COMMAND ====================
void executeManualCommand() {
  if (manualCommand == "FORWARD") forward(MANUAL_SPEED);
  else if (manualCommand == "BACKWARD") backward(MANUAL_SPEED);
  else if (manualCommand == "LEFT") turnLeft(MANUAL_TURN_SPEED);
  else if (manualCommand == "RIGHT") turnRight(MANUAL_TURN_SPEED);
  else if (manualCommand == "STOP") stopRobot();
}

// ==================== SCAN FUNCTION ====================
void scanForHuman() {
  if (isScanning) return;
  isScanning = true;
  currentAction = "SCANNING";
  
  Serial.println("\n🔍 Scanning for human...");
  stopRobot();
  delay(100);
  
  int leftDist, centerDist, rightDist;
  
  myServo.write(SERVO_LEFT);
  delay(SCAN_DELAY);
  leftDist = getFilteredDistance();
  
  myServo.write(SERVO_CENTER);
  delay(SCAN_DELAY);
  centerDist = getFilteredDistance();
  
  myServo.write(SERVO_RIGHT);
  delay(SCAN_DELAY);
  rightDist = getFilteredDistance();
  
  myServo.write(SERVO_CENTER);
  
  if (leftDist < centerDist && leftDist < rightDist && leftDist < ZONE_LOST && leftDist > 10) {
    turnLeft(SPEED_TURN);
    delay(TURN_DURATION);
  }
  else if (rightDist < centerDist && rightDist < leftDist && rightDist < ZONE_LOST && rightDist > 10) {
    turnRight(SPEED_TURN);
    delay(TURN_DURATION);
  }
  else if (centerDist < ZONE_LOST && centerDist > 10) {
    forward(SPEED_NORMAL);
    delay(400);
  }
  else {
    turnLeft(SPEED_TURN);
    delay(500);
  }
  
  stopRobot();
  isScanning = false;
}

// ==================== SEND DATA TO DASHBOARD ====================
void sendSensorData() {
  if (WiFi.status() != WL_CONNECTED) return;
  if (millis() - lastSensorSend < 80) return;
  
  totalReadings++;
  avgDist = ((avgDist * (totalReadings - 1)) + currentDistance) / totalReadings;
  if (currentDistance > maxDist && currentDistance < 300) maxDist = currentDistance;
  if (currentDistance < minDist && currentDistance > 0) minDist = currentDistance;
  
  String json = "{\"dist\":" + String(currentDistance) +
                ",\"speed\":" + String(currentSpeed) +
                ",\"action\":\"" + currentAction + 
                "\",\"mode\":\"" + currentMode +
                "\",\"max\":" + String(maxDist) +
                ",\"min\":" + String(minDist) +
                ",\"avg\":" + String(avgDist) + "}";
  webSocket.broadcastTXT(json);
  lastSensorSend = millis();
}

// ==================== OLED DISPLAY ====================
void updateOLED() {
  if (!oledWorking) return;
  if (millis() - lastOLEDupdate < OLED_UPDATE_DELAY) return;
  
  display.clearDisplay();
  
  // Draw border
  display.drawRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, SSD1306_WHITE);
  
  // Distance (Big font)
  display.setTextSize(2);
  display.setCursor(5, 5);
  display.print(currentDistance);
  display.setTextSize(1);
  display.print(" cm");
  
  // Mode
  display.setCursor(5, 25);
  display.print("Mode:");
  display.print(currentMode);
  
  // Speed
  display.setCursor(5, 37);
  display.print("Spd:");
  display.print(currentSpeed);
  
  // Action
  display.setCursor(5, 49);
  display.print("Act:");
  if (currentAction.length() > 8) {
    display.print(currentAction.substring(0, 8));
  } else {
    display.print(currentAction);
  }
  
  display.display();
  lastOLEDupdate = millis();
}

// ==================== AUTO MODE ====================
void autoMode() {
  if (currentDistance > ZONE_LOST || currentDistance < 5) {
    if (!isScanning) {
      stopRobot();
      lostCounter++;
      if (lostCounter >= LOST_TIMEOUT) {
        scanForHuman();
        lostCounter = 0;
      }
    }
    return;
  }
  lostCounter = 0;
  
  if (currentDistance < ZONE_CRITICAL) {
    backward(SPEED_BACK_FAST);
    delay(BACK_DURATION);
  }
  else if (currentDistance < ZONE_TOO_CLOSE) {
    backward(SPEED_BACK_NORMAL);
  }
  else if (currentDistance < ZONE_MAINTAIN) {
    int speed = map(currentDistance, ZONE_TOO_CLOSE, ZONE_MAINTAIN, SPEED_BACK_NORMAL, SPEED_MAINTAIN);
    forward(speed);
  }
  else if (currentDistance <= ZONE_IDEAL + 2) {
    int speed = map(currentDistance, ZONE_MAINTAIN, ZONE_IDEAL + 2, SPEED_MAINTAIN, SPEED_IDEAL);
    forward(speed);
  }
  else if (currentDistance <= ZONE_FOLLOW) {
    int speed = map(currentDistance, ZONE_IDEAL + 2, ZONE_FOLLOW, SPEED_IDEAL, SPEED_NORMAL);
    forward(speed);
  }
  else if (currentDistance <= ZONE_FAR) {
    int speed = map(currentDistance, ZONE_FOLLOW, ZONE_FAR, SPEED_NORMAL, SPEED_FAST);
    forward(speed);
  }
  else if (currentDistance <= ZONE_VERY_FAR) {
    int speed = map(currentDistance, ZONE_FAR, ZONE_VERY_FAR, SPEED_FAST, SPEED_VERY_FAST);
    forward(speed);
  }
  else if (currentDistance < ZONE_LOST) {
    int speed = map(currentDistance, ZONE_VERY_FAR, ZONE_LOST, SPEED_VERY_FAST, SPEED_MAX);
    forward(speed);
  }
}

// ==================== MANUAL MODE ====================
void manualMode() {
  executeManualCommand();
}

// ==================== MAIN FOLLOWING LOGIC ====================
void followHuman() {
  currentDistance = getFilteredDistance();
  sendSensorData();
  updateOLED();
  
  if (millis() - lastPrint > 500) {
    Serial.print("📏 Dist: ");
    Serial.print(currentDistance);
    Serial.print(" cm | Mode: ");
    Serial.print(currentMode);
    Serial.print(" | Speed: ");
    Serial.print(currentSpeed);
    Serial.print(" | Action: ");
    Serial.println(currentAction);
    lastPrint = millis();
  }
  
  if (currentMode == "AUTO") {
    autoMode();
  } else {
    manualMode();
  }
}

// ==================== MAIN LOOP ====================
void loop() {
  if (WiFi.status() == WL_CONNECTED) {
    webSocket.loop();
    server.handleClient();
  }
  followHuman();
  delay(LOOP_DELAY);
}