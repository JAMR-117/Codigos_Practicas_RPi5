# Códigos de Prácticas - Raspberry Pi 5 & ESP32
## Proyecto PAPIME PE113226: Implementación de Prácticas Didácticas con Raspberry Pi para el Aprovechamiento Sustentable de la Energía Solar
**Facultad de Estudios Superiores Aragón - UNAM**

Este repositorio contiene los códigos fuente íntegros extraídos de los manuales de la **Práctica 5** y **Práctica 6**, diseñados para su despliegue y ejecución en una **Raspberry Pi 5** y un microcontrolador **ESP32**.

---

## Estructura del Repositorio

```
Codigos_Practicas_RPi5/
├── Practica_5/
│   ├── app.py                     # Servidor Web Flask con cliente MQTT
│   ├── Dockerfile                 # Imagen Docker ARM64 para Flask
│   ├── docker-compose.yml         # Orquestación de Flask y Nginx en modo host
│   ├── requirements.txt           # Dependencias Python (Flask, paho-mqtt)
│   ├── nginx/
│   │   └── nginx.conf             # Configuración de proxy inverso Nginx (puerto 80 -> 5000)
│   └── templates/
│       └── index.html             # Interfaz web dinámica para visualización y control
│
├── Practica_6/
│   ├── seguidor_solar_esp32.ino   # Firmware Arduino C++ para ESP32 (LDRs, PCA9685 y MQTT)
│   ├── esp32_seguidor_solar/
│   │   └── esp32_seguidor_solar.ino # Copia en carpeta de sketch para Arduino IDE
│   ├── spa_controller.py          # Microservicio de cálculo astronómico con pvlib y MQTT
│   ├── Dockerfile                 # Imagen Docker ARM64 para el algoritmo SPA
│   ├── docker-compose.yml         # Orquestación del contenedor algoritmo_spa
│   └── requirements.txt           # Dependencias Python (pandas, pvlib, paho-mqtt<2.0.0)
│
├── .gitignore
└── README.md
```

---

## Descripción de las Prácticas

### Práctica 5: Desarrollo de Interfaces Web
- **Backend**: Servidor Flask (`app.py`) suscrito a `seguidor/#` para procesar coordenadas de Azimut y Cenit, calcular la energía teórica ahorrada y proveer endpoints REST (`/api/seguidores` y `/api/modo`).
- **Frontend**: Dashboard reactivo (`index.html`) con JavaScript asíncrono para actualización en tiempo real de tarjetas de seguidores solares.
- **Proxy Inverso**: Nginx redirigiendo peticiones externas en el puerto estándar 80 hacia el puerto 5000.
- **Despliegue**:
  ```bash
  cd Practica_5
  sudo docker compose up --build -d
  ```

### Práctica 6: Seguimiento Solar en Dos Ejes (LDR y Algoritmo SPA)
- **ESP32 Edge Computing**: Lectura diferencial de 4 fotorresistencias (Arriba, Abajo, Izquierda, Derecha), control de dos servos vía PCA9685 (I2C) y telemetría periódica hacia el Broker MQTT.
- **Control SPA (Solar Position Algorithm)**: Microservicio en Python utilizando la librería `pvlib` para calcular la posición astronómica del sol según coordenadas geográficas (FES Aragón: 19.4755° N, -99.0443° W) y posicionar los servomotores con precisión matemática.
- **Despliegue del microservicio SPA**:
  ```bash
  cd Practica_6
  sudo docker compose up --build -d
  ```
