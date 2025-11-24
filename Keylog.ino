#include <LiquidCrystal.h>
#include <SoftwareSerial.h>

// TU LCD REAL SEGÚN DIAGRAMA:
LiquidCrystal lcd(12, 11, 5, 4, 3, 2);

// Bluetooth por SoftwareSerial
// RX del Arduino ← TX del HC-05 en pin 7
// TX del Arduino → RX del HC-05 en pin 8
SoftwareSerial BT(7, 8);

// Control del LCD
int col = 0;
int row = 0;

void setup() {
  Serial.begin(9600);   // USB desde keylog.py
  BT.begin(9600);       // Bluetooth

  lcd.begin(16, 2);
  lcd.clear();
  lcd.print("Recibiendo...");
  delay(1000);
  lcd.clear();
}

void loop() {
  // Datos desde Python
  if (Serial.available()) {
    char c = Serial.read();

    // Reenviar por Bluetooth (enviar inmediatamente)
    BT.write(c);
    
    // Pequeño delay para que Bluetooth transmita
    delay(2);

    // Mostrar en LCD
    imprimirLCD(c);
  }
}

void imprimirLCD(char c) {

  // ignorar saltos de línea
  if (c == '\n' || c == '\r')
    return;

  // Caracter especial: inicio de token <XX>
  if (c == '<') {
    String token = "<";
    
    // Leer el resto del token con timeout
    unsigned long timeout = millis() + 50;  // 50ms timeout
    while (millis() < timeout) {
      if (Serial.available()) {
        char t = Serial.read();
        token += t;
        
        // Reenviar también este caracter por Bluetooth
        BT.write(t);
        delay(2);
        
        if (t == '>') break;
      }
    }

    // ENTER
    if (token == "<EN>") {
      row = (row + 1) % 2;  // pasa a siguiente fila
      col = 0;
      lcd.setCursor(col, row);
      return;
    }

    // BACKSPACE
    if (token == "<BK>") {
      if (col > 0) {
        col--;
      } else {
        // Si estamos al inicio, limpiar fila
        lcd.setCursor(0, row);
        lcd.print("                ");
        col = 0;
      }
      lcd.setCursor(col, row);
      lcd.print(" ");
      lcd.setCursor(col, row);
      return;
    }

    return;  // otros tokens se ignoran por ahora
  }

  // Imprimir caracter normal
  lcd.setCursor(col, row);
  lcd.print(c);

  col++;
  if (col >= 16) {   // fin de línea
    col = 0;
    row++;
    if (row > 1) {
      row = 0;
      lcd.clear();
    }
  }
}
