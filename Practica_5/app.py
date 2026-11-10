from flask import Flask, render_template, request
import paho.mqtt.client as mqtt
import time

app = Flask(__name__)

sistemas_detectados = {}
TIEMPO_INACTIVIDAD = 5
POTENCIA_AHORRO_W = 0.6

client = mqtt.Client()
def on_message(client, userdata, msg):
    try:
        payload = msg.payload.decode()
        partes = msg.topic.split('/')
       
        if len(partes) >= 3 and partes[0] == "seguidor" and partes[1].isdigit():
            num_sistema = partes[1]
            eje_o_parametro = partes[2]
            id_sistema = f"seguidor_{num_sistema}"
            tiempo_actual = time.time()
           
            if id_sistema not in sistemas_detectados:
                sistemas_detectados[id_sistema] = {
                    "eje_x": 90, "eje_y": 90, "energia_ahorrada": 0.0,
                    "status": "Online", "modo": "Sensores (LDR)", "last_seen": tiempo_actual
                }
           
            try:
                valor = int(float(payload))
                if eje_o_parametro == 'azimut':
                    sistemas_detectados[id_sistema]["eje_x"] = valor
                elif eje_o_parametro == 'cenit':
                    sistemas_detectados[id_sistema]["eje_y"] = valor
            except ValueError:
                pass
               
            delta_segundos = tiempo_actual - sistemas_detectados[id_sistema]["last_seen"]
            if delta_segundos > 0:
                sistemas_detectados[id_sistema]["energia_ahorrada"] += POTENCIA_AHORRO_W * (delta_segundos / 3600.0)
               
            sistemas_detectados[id_sistema]["status"] = "Online"
            sistemas_detectados[id_sistema]["last_seen"] = tiempo_actual
           
    except Exception as e:
        pass



client.on_message = on_message
client.connect("127.0.0.1", 1883, 60)
client.subscribe("seguidor/#")
client.loop_start()

@app.route('/')
def index():
    return render_template('index.html')

@app.route('/api/seguidores')
def get_data():
    tiempo_actual = time.time()
    lineas = []
    for id_sistema, info in sistemas_detectados.items():
        if (tiempo_actual - info.get("last_seen", 0)) > TIEMPO_INACTIVIDAD:
            info["status"] = "Offline"
        linea = f"{id_sistema},{info['eje_x']},{info['eje_y']},{info['energia_ahorrada']:.5f},{info['status']},{info['modo']}"
        lineas.append(linea)
    return "\n".join(lineas)

@app.route('/api/modo', methods=['POST'])
def cambiar_modo():
    id_sistema = request.form.get('id_sistema')
    nuevo_modo = request.form.get('modo')
    if id_sistema in sistemas_detectados:
        sistemas_detectados[id_sistema]["modo"] = nuevo_modo
        num_sistema = id_sistema.split('_')[1]
        client.publish(f"seguidor/{num_sistema}/modo", nuevo_modo)
    return "ok"

if __name__ == '__main__':
    app.run(host='0.0.0.0', port=5000)
