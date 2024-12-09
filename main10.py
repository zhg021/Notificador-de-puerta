from machine import Pin, PWM
import time
import network
import socket
import _thread  # Para manejar múltiples hilos
import ssl
import ujson

# Variables globales
correo = "ejemplo@gmail.com"
ubicacion = ""
estado_interruptor = "Desconocido"  # Almacena el estado del interruptor
ip_servidor = '201.171.5.201'
puerto_servidor = 1997

# Configuracion del pin de interrupción
INTERRUPT_PIN = 13
interrupt_pin = Pin(INTERRUPT_PIN, Pin.IN, Pin.PULL_UP)

BUZZER_GPIO = 4
buzzer_pwm = PWM(Pin(BUZZER_GPIO))

cmd = "cmd"
estado = ""

# Variables para controlar el estado del interruptor
one_time1 = True
one_time2 = False

def buzz():
    """
    Configura y activa el buzzer con una frecuencia de 1 kHz y un ciclo de trabajo del 50%.
    """
    # Configurar la frecuencia a 1 kHz
    buzzer_pwm.freq(1000)
    # Configurar el duty cycle al 50% (máximo de 1023 en MicroPython)
    buzzer_pwm.duty(512)
    print("Buzzer activado")

def stop_buzz():
    """
    Detiene el buzzer poniendo el ciclo de trabajo a 0.
    """
    # Configurar el duty cycle a 0 para detener el buzzer
    buzzer_pwm.duty(0)
    print("Buzzer detenido")

# Configurar el ESP32 como Access Point
def configurar_ap():
    ap = network.WLAN(network.AP_IF)
    ap.active(True)
    ssid = "ConfigAP"
    password = "12345678"  # Minimo 8 caracteres
    ap.config(essid=ssid, password=password)
    ap.ifconfig(('192.168.4.1', '255.255.255.0', '192.168.4.1', '8.8.8.8'))
    print("Access Point activado:")
    print("  SSID:", ssid)
    print("  IP:", ap.ifconfig()[0])
    return ap

# Configurar Wi-Fi en modo cliente
def configurar_wifi(ssid, password):
    station = network.WLAN(network.STA_IF)
    station.active(True)
    station.connect(ssid, password)
    
    print("Conectando a la red Wi-Fi:", ssid)
    for _ in range(10):  # Intentar durante 10 segundos
        if station.isconnected():
            print("Conexión exitosa:", station.ifconfig())
            return station
        time.sleep(1)
    
    print("Error al conectar a la red Wi-Fi.")
    return None

def send_tcp_message(cmd, email, ubicacion, estado, tcp_server_ip, tcp_server_port):
    # Crear el mensaje
    message = f"{cmd}:{email}:{ubicacion}:{estado}"
    
    # Configurar la dirección del servidor
    dest_addr = (tcp_server_ip, tcp_server_port)
    
    try:
        # Crear el socket
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        print("Socket creado con éxito")
        
        # Conectar al servidor
        sock.connect(dest_addr)
        print(f"Conectado al servidor {tcp_server_ip}:{tcp_server_port}")
        
        # Enviar el mensaje
        sock.sendall(message.encode('utf-8'))
        print(f"Mensaje enviado: {message}")
    except Exception as e:
        print(f"Error al enviar el mensaje: {e}")
    finally:
        # Cerrar el socket
        sock.close()
        print("Socket cerrado")

# Hilo para monitorear el estado del interruptor
def leer_estado_interruptor():
    global one_time1, one_time2, estado_interruptor
    while True:
        switch_state = interrupt_pin.value()
        if switch_state == 1:
            if one_time1:
                estado_interruptor = "Cerrado"
                print("Interruptor cerrado")
                one_time1 = False
                one_time2 = True
                
                estado = 'Close'
                send_tcp_message(cmd,correo,ubicacion,estado,ip_servidor,puerto_servidor)
                #stop_buzz()
            
        else:
            if one_time2:
                estado_interruptor = "Abierto"
                print("Interruptor abierto")
                one_time2 = False
                one_time1 = True
                
                estado = 'Open'
                send_tcp_message(cmd,correo,ubicacion,estado,ip_servidor,puerto_servidor)
                #enviar_tcp(ip_servidor, puerto_servidor, "Open", correo, ubicacion, "Open")
                #buzz()
             
        time.sleep(0.5)  # Evitar lecturas demasiado frecuentes

# Servidor UDP para recibir comandos
def servidor_udp():
    global correo, ubicacion, estado_interruptor  # Acceso a las variables globales
    
    udp_socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    udp_socket.bind(('0.0.0.0', 12345))  # Escuchar en el puerto 12345
    print("Servidor UDP activo en el puerto 12345, esperando mensajes...")
    
    station = None  # Para guardar la instancia de Wi-Fi cliente
    while True:
        data, addr = udp_socket.recvfrom(1024)  # Recibir datos
        mensaje = data.decode().strip()
        print("Mensaje recibido de:", addr, "Contenido:", mensaje)
        
        # Procesar comandos recibidos
        if mensaje.startswith("CONFIG:wifi:"):
            try:
                _, _, ssid, password = mensaje.split(":")
                print("Configurando Wi-Fi con SSID:", ssid, "y contraseña:", password)
                station = configurar_wifi(ssid, password)
                if station:
                    udp_socket.sendto(b"ACK:WiFi conectado", addr)
                else:
                    udp_socket.sendto(b"NACK:Error al conectar WiFi", addr)
            except Exception as e:
                print("Error al procesar mensaje Wi-Fi:", e)
                udp_socket.sendto(b"NACK:Formato WiFi invalido", addr)
        
        elif mensaje.startswith("GET:interruptor"):
            # Responder con el estado actual del interruptor
            respuesta = f"Estado del interruptor: {estado_interruptor}"
            udp_socket.sendto(respuesta.encode(), addr)
        
        elif mensaje.startswith("CONFIG:correo:"):
            try:
                _, _, nuevo_correo = mensaje.split(":")
                correo = nuevo_correo
                print("Correo configurado a:", correo)
                udp_socket.sendto(b"ACK:Correo configurado", addr)
            except Exception as e:
                print("Error al procesar mensaje de correo:", e)
                udp_socket.sendto(b"NACK:Formato correo invalido", addr)
        
        elif mensaje.startswith("CONFIG:ubicacion:"):
            try:
                _, _, nueva_ubicacion = mensaje.split(":")
                ubicacion = nueva_ubicacion
                print("Ubicación configurada a:", ubicacion)
                udp_socket.sendto(b"ACK:Ubicación configurada", addr)
            except Exception as e:
                print("Error al procesar mensaje de ubicación:", e)
                udp_socket.sendto(b"NACK:Formato ubicacion invalido", addr)
        
        else:
            udp_socket.sendto(b"NACK:Comando no reconocido", addr)

# Programa principal
def main():
    ap = configurar_ap()  # Configurar el Access Point
    _thread.start_new_thread(leer_estado_interruptor, ())  # Iniciar monitoreo del interruptor en un hilo
    servidor_udp()  # Iniciar el servidor UDP
    ap.active(False)  # Desactivar el modo AP después de configurar Wi-Fi
    print("Modo AP desactivado. Dispositivo conectado a Wi-Fi.")

if __name__ == "__main__":
    main()
