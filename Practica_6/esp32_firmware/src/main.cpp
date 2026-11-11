#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <Wire.h>
#include <Adafruit_PWMServoDriver.h>

// ==========================================
// 1. CONFIGURACIÓN DE RED Y BROKER MQTT
// ==========================================
// NOTA: El microcontrolador ESP32 opera en la banda Wi-Fi de 2.4 GHz.
// Se preconfigura con las credenciales de la red local (2.4 GHz):
const char* ssid = "Logan_2.4";
const char* password = "L0g4n 2o25";
const char* mqtt_server = "192.168.1.64"; // IP de la Raspberry Pi

// ==========================================
// 2. ASIGNACIÓN DE PINES (ADC ESP32)
// ==========================================
#define LDR_ARRIBA 34
#define LDR_ABAJO  35
#define LDR_IZQ    32
#define LDR_DER    33

// ==========================================
// 3. CONFIGURACIÓN PCA9685 Y SERVOMOTORES
// ==========================================
Adafruit_PWMServoDriver pca = Adafruit_PWMServoDriver(0x40);

// Calibración de pulsos para servomotores a 50Hz (Estándar SG90 / MG995 / MG996R)
#define SERVO_MIN 100 // Pulso mínimo correspondiente a 0°
#define SERVO_MAX 500 // Pulso máximo correspondiente a 180°

WiFiClient espClient;
PubSubClient client(espClient);

// Variables de posición angular (Iniciadas al centro: 90°)
int angulo_x = 90; // Azimut (Servo canal 0)
int angulo_y = 90; // Cenit   (Servo canal 1)

// Modo operativo: "Sensores (LDR)" o "Algoritmo (SPA)"
String modo_actual = "Sensores (LDR)";

// Control de tiempo para Telemetría / Heartbeat periódico (evita falso Offline)
unsigned long ultimo_envio_telemetria = 0;
const unsigned long INTERVALO_TELEMETRIA = 3000; // Cada 3 segundos

// ==========================================
// 4. FUNCIONES AUXILIARES
// ==========================================
int convertirAngulo(int angulo) {
  int angulo_seguro = constrain(angulo, 0, 180);
  return map(angulo_seguro, 0, 180, SERVO_MIN, SERVO_MAX);
}

void publicarTelemetria() {
  client.publish("seguidor/1/azimut", String(angulo_x).c_str());
  client.publish("seguidor/1/cenit", String(angulo_y).c_str());
}

// ==========================================
// 5. CALLBACK DE RECEPCIÓN MQTT
// ==========================================
void callback(char* topic, byte* payload, unsigned int length) {
  String mensaje = "";
  for (unsigned int i = 0; i < length; i++) {
    mensaje += (char)payload[i];
  }
  mensaje.trim(); // Elimina posibles retornos de carro o espacios

  // 1. Conmutación de modo solicitada desde el Servidor Web
  if (strcmp(topic, "seguidor/1/modo") == 0) {
    modo_actual = mensaje;
    Serial.println("[MQTT] Cambio de modo recibido: " + modo_actual);
  }

  // 2. Recepción de órdenes angulares en modo Algoritmo SPA
  if (modo_actual == "Algoritmo (SPA)") {
    if (strcmp(topic, "seguidor/1/control/azimut") == 0) {
      angulo_x = constrain(mensaje.toInt(), 0, 180);
      pca.setPWM(0, 0, convertirAngulo(angulo_x));
      client.publish("seguidor/1/azimut", String(angulo_x).c_str());
      Serial.printf("[SPA] Nuevo Azimut aplicado: %d°\n", angulo_x);
    }
    if (strcmp(topic, "seguidor/1/control/cenit") == 0) {
      angulo_y = constrain(mensaje.toInt(), 0, 180);
      pca.setPWM(1, 0, convertirAngulo(angulo_y));
      client.publish("seguidor/1/cenit", String(angulo_y).c_str());
      Serial.printf("[SPA] Nuevo Cenit aplicado: %d°\n", angulo_y);
    }
  }
}

// ==========================================
// 6. GESTIÓN DE RECONEXIÓN ROBUSTA
// ==========================================
void reconnect() {
  while (!client.connected()) {
    // Si la conexión WiFi se interrumpió, reconectar primero
    if (WiFi.status() != WL_CONNECTED) {
      Serial.print("Reconectando a WiFi...");
      WiFi.disconnect();
      WiFi.reconnect();
      int intentos_wifi = 0;
      while (WiFi.status() != WL_CONNECTED && intentos_wifi < 20) {
        delay(500);
        Serial.print(".");
        intentos_wifi++;
      }
      if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\nWiFi restablecido. IP: " + WiFi.localIP().toString());
      } else {
        Serial.println("\nFallo en reconexión WiFi. Reintentando...");
        delay(2000);
        continue;
      }
    }

    Serial.print("Conectando al Broker MQTT en ");
    Serial.print(mqtt_server);
    Serial.print("...");
    if (client.connect("ESP32_Seguidor_1")) {
      Serial.println(" ¡Conectado exitosamente!");
     
      // Suscripción a los tópicos de control
      client.subscribe("seguidor/1/modo");
      client.subscribe("seguidor/1/control/azimut");
      client.subscribe("seguidor/1/control/cenit");
     
      // Publicación de estado inicial para activar la tarjeta en la interfaz web
      publicarTelemetria();
    } else {
      Serial.printf(" Falló (rc=%d). Reintentando en 2 segundos...\n", client.state());
      delay(2000);
    }
  }
}

// ==========================================
// 7. SETUP
// ==========================================
void setup() {
  Serial.begin(115200);
  delay(100);
  Serial.println("\n--- INICIALIZANDO SEGUIDOR SOLAR ESP32 ---");

  // Configuración del bus I2C y PCA9685
  Wire.begin();
  pca.begin();
  pca.setPWMFreq(50); // Frecuencia estándar para servos analógicos y digitales

  // Posicionar servomotores al centro (90°)
  pca.setPWM(0, 0, convertirAngulo(angulo_x));
  pca.setPWM(1, 0, convertirAngulo(angulo_y));
  Serial.println("Servos calibrados en posición neutra (90°, 90°).");

  // Conexión Wi-Fi
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.print("Conectando a red Wi-Fi: ");
  Serial.println(ssid);
  
  int intentos = 0;
  while (WiFi.status() != WL_CONNECTED && intentos < 30) {
    delay(500);
    Serial.print(".");
    intentos++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nConexión Wi-Fi establecida.");
    Serial.print("Dirección IP asignada: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.printf("\nAdvertencia: No se pudo conectar a '%s' (código estado: %d).\n", ssid, WiFi.status());
    Serial.println("Continuando inicialización de periféricos y bucle de control...");
  }

  // Configuración del cliente MQTT
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
}

// ==========================================
// 8. LOOP PRINCIPAL
// ==========================================
void loop() {
  if (WiFi.status() == WL_CONNECTED) {
    if (!client.connected()) {
      reconnect();
    }
    client.loop();

    // Enviar telemetría periódica (Heartbeat) para mantener el estado "Online" en la Web
    unsigned long tiempo_actual = millis();
    if (tiempo_actual - ultimo_envio_telemetria >= INTERVALO_TELEMETRIA) {
      ultimo_envio_telemetria = tiempo_actual;
      publicarTelemetria();
    }
  }

  // MODO AUTÓNOMO: Seguimiento por diferencial analógico de fotorresistencias
  if (modo_actual == "Sensores (LDR)") {
    int val_arriba = analogRead(LDR_ARRIBA);
    int val_abajo  = analogRead(LDR_ABAJO);
    int val_izq    = analogRead(LDR_IZQ);
    int val_der    = analogRead(LDR_DER);

    int dif_vert  = val_arriba - val_abajo;
    int dif_horiz = val_izq - val_der;

    int tolerancia = 200; // Banda muerta para mitigar oscilaciones mecánicas
    bool se_movio = false;

    // Control Eje Cenit (Vertical - Servo canal 1)
    if (abs(dif_vert) > tolerancia) {
      if (val_arriba > val_abajo && angulo_y < 180) angulo_y++;
      if (val_abajo > val_arriba && angulo_y > 0)   angulo_y--;
      pca.setPWM(1, 0, convertirAngulo(angulo_y));
      se_movio = true;
    }

    // Control Eje Azimut (Horizontal - Servo canal 0)
    if (abs(dif_horiz) > tolerancia) {
      if (val_izq > val_der && angulo_x < 180) angulo_x++;
      if (val_der > val_izq && angulo_x > 0)   angulo_x--;
      pca.setPWM(0, 0, convertirAngulo(angulo_x));
      se_movio = true;
    }

    // Si hubo desplazamiento, notificar de inmediato por MQTT
    if (se_movio) {
      if (WiFi.status() == WL_CONNECTED && client.connected()) {
        publicarTelemetria();
      }
      delay(40); // Suavidad de transición angular
    }
  }
}
