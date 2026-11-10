import os
import sys
import time
import pandas as pd
from pvlib.location import Location
import paho.mqtt.client as mqtt

# ==========================================
# 1. CONFIGURACIÓN DEL BROKER MQTT
# ==========================================
# Al utilizar network_mode: "host" en docker-compose, el contenedor comparte
# la red local de la Raspberry Pi, permitiendo conectar directamente a localhost.
BROKER = os.getenv("MQTT_BROKER", "127.0.0.1")
PUERTO = 1883

TOPIC_MODO = "seguidor/1/modo"
TOPIC_AZIMUT = "seguidor/1/control/azimut"
TOPIC_CENIT = "seguidor/1/control/cenit"

modo_actual = "Sensores (LDR)"

# ==========================================
# 2. CONFIGURACIÓN GEOGRÁFICA ASTRONÓMICA
# ==========================================
# Coordenadas: FES Aragón, UNAM (Nezahualcóyotl / CDMX)
# Latitud: 19.4755° N, Longitud: -99.0443° W, Altitud: 2240 msnm
coordenadas = Location(
    latitude=19.4755,
    longitude=-99.0443,
    tz='America/Mexico_City',
    altitude=2240,
    name='FES_Aragon_UNAM'
)

# ==========================================
# 3. CALLBACK DE MENSAJERÍA MQTT
# ==========================================
def on_message(client, userdata, msg):
    global modo_actual
    try:
        payload = msg.payload.decode().strip()
        if msg.topic == TOPIC_MODO:
            modo_actual = payload
            print(f"[MQTT] Modo de operación actualizado a: '{modo_actual}'", flush=True)
    except Exception as e:
        print(f"[ERROR] Error al procesar mensaje MQTT: {e}", flush=True)

# Instanciación compatible con paho-mqtt versión 1.x y 2.x
try:
    client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION1)
except AttributeError:
    client = mqtt.Client()

client.on_message = on_message

print(f"Conectando al Broker MQTT en {BROKER}:{PUERTO}...", flush=True)
try:
    client.connect(BROKER, PUERTO, 60)
except Exception as e:
    print(f"[FATAL] No se pudo conectar al Broker MQTT: {e}. Verifique que el servicio Mosquitto esté activo.", flush=True)
    sys.exit(1)

client.subscribe(TOPIC_MODO)
client.loop_start()

print("==================================================================", flush=True)
print(" Controlador SPA (Solar Position Algorithm) iniciado exitosamente.", flush=True)
print(" Esperando instrucciones de conmutación desde la interfaz Web...", flush=True)
print("==================================================================", flush=True)

# ==========================================
# 4. CICLO DE CÁLCULO ASTRONÓMICO
# ==========================================
try:
    while True:
        if modo_actual == "Algoritmo (SPA)":
            # 1. Obtener estampa de tiempo local en tiempo real
            tiempo_actual = pd.Timestamp.now(tz='America/Mexico_City')
           
            # 2. Computar efemérides solares precisas
            sol = coordenadas.get_solarposition(tiempo_actual)
            azimut_real = float(sol['azimuth'].iloc[0])
            cenit_real = float(sol['apparent_zenith'].iloc[0])
            elevacion_real = 90.0 - cenit_real

            # 3. Mapeo cinemático para el seguidor solar de dos ejes:
            # Azimut: Mapeo angular proporcional a los 180° del servomotor mecánico
            angulo_x = int((azimut_real / 360.0) * 180.0)
           
            # Cenit / Elevación: Elevación solar restringida al semiplano superior [0°, 90°]
            angulo_y = int(elevacion_real)
           
            # 4. Acotar estrictamente dentro del rango físico del hardware [0°, 180°]
            angulo_x = max(0, min(180, angulo_x))
            angulo_y = max(0, min(180, angulo_y))

            # 5. Publicar consignas angulares hacia la ESP32
            client.publish(TOPIC_AZIMUT, str(angulo_x))
            client.publish(TOPIC_CENIT, str(angulo_y))

            print(f"[SPA ACTIVO] Azimut Sol: {azimut_real:.2f}° -> Servo X: {angulo_x}° | "
                  f"Elevación Sol: {elevacion_real:.2f}° -> Servo Y: {angulo_y}°", flush=True)
           
            # Período de actualización astronómica (5 segundos)
            time.sleep(5)
        else:
            # En modo Sensores (LDR), el microservicio permanece en reposo activo
            time.sleep(1)

except KeyboardInterrupt:
    print("\nDeteniendo microservicio Algoritmo SPA...", flush=True)
    client.loop_stop()
    client.disconnect()
    print("Microservicio detenido limpiamente.", flush=True)
