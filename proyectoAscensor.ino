#include <Keypad.h>
#include <LiquidCrystal.h>
#include <LedControl.h>
#define buzzerPin PA1
#define ledBloqueoPin PA10 

bool bloqueado = false;
unsigned long tiempoBloqueo = 0;

const byte ROWS = 4;
const byte COLS = 4;
char keys[ROWS][COLS] = {
  {'1','2','3','A'},  // P1, P2, P3, Abrir
  {'4','5','6','B'},  // P4, P5, P6, Cerrar
  {'7','8','9','C'},  // P7, P8, P9, Bloqueo
  {'*','0','#','D'}   // SB1, PB0, SB2, Alarma
};
byte rowPins[ROWS] = {PA2, PA3, PA4, PA5};  
byte colPins[COLS] = {PA6, PA7, PA8, PA9};  
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

LiquidCrystal lcd(PB0, PB1, PB2, PB3, PB4, PB5);  

LedControl matrixArriba = LedControl(PB6, PB7, PB8, 1);  // DIN, CLK, CS 
LedControl matrixAbajo  = LedControl(PB9, PB10, PB11, 1); 

//Configuración de las LED Matrices para pisos
#define LED_DIN PC0
#define LED_CLK PC1
const int csPins[12] = {PC2, PC3, PC5, PC6, PC7, PC8, PC9, PC10, PC11, PC12, PC13, PB9};

// Arreglo de punteros a objetos LedControl para cada piso
LedControl* floorMatrices[12];

// Estado de cada LED Matrix (si ya se alcanzó ese piso)
bool estadoMatrices[12] = {false, false, false, false, false, false, false, false, false, false, false, false};

// Índice 0: SB2, 1: SB1, 2: PB0, 3: P1, 4: P2, 5: P3, 6: P4, 7: P5, 8: P6, 9: P7, 10: P8, 11: P9
byte patternSB2[8] = {B11111111, B10000001, B10111101, B10000101, B10001001, B10111101, B10000001, B11111111};  
byte patternSB1[8] = {B11111111, B10000001, B10001101, B10000101, B10000101, B10000101, B10000001, B11111111};  
byte patternPB0[8] = {B00111100, B01000010, B01000010, B01000010, B01000010, B01000010, B00111100, B00000000};  
byte patternP1[8]  = {B00010000, B00110000, B00010000, B00010000, B00010000, B00010000, B00111000, B00000000};  
byte patternP2[8]  = {B00111100, B01000010, B00000100, B00001000, B00010000, B00100000, B01111110, B00000000};  
byte patternP3[8]  = {B00111100, B01000010, B00000100, B00011100, B00000100, B01000010, B00111100, B00000000};  
byte patternP4[8]  = {B00001000, B00011000, B00101000, B01001000, B01111110, B00001000, B00001000, B00000000};  
byte patternP5[8]  = {B01111110, B01000000, B01111100, B00000010, B00000010, B01000010, B00111100, B00000000};  
byte patternP6[8]  = {B00111100, B01000010, B01000000, B01111100, B01000010, B01000010, B00111100, B00000000};  
byte patternP7[8]  = {B01111110, B00000010, B00000100, B00001000, B00010000, B00010000, B00010000, B00000000};  
byte patternP8[8]  = {B00111100, B01000010, B00111100, B01000010, B01000010, B01000010, B00111100, B00000000};  
byte patternP9[8]  = {B00111100, B01000010, B01000010, B00111110, B00000010, B01000010, B00111100, B00000000};

byte* floorPatterns[12] = { patternSB2, patternSB1, patternPB0, patternP1, patternP2, patternP3, patternP4, patternP5, patternP6, patternP7, patternP8, patternP9 };

int pisoActual = 0;  // Inicia en Planta Baja
int direccion = 0;   // 0: Detenido, 1: Arriba, -1: Abajo
bool puertaAbierta = false;
unsigned long tiempoMovimiento = 0;

byte arrowUp[8] = {0x18, 0x3C, 0x7E, 0xFF, 0x18, 0x18, 0x18, 0x18};
byte arrowDown[8] = {0x18, 0x18, 0x18, 0x18, 0xFF, 0x7E, 0x3C, 0x18};

int listaDestinos[10] = {0};  

// Función para mostrar un patrón en una LED Matrix
void mostrarPatron(LedControl* matrix, byte* pattern) {
  for (int row = 0; row < 8; row++) {
    matrix->setRow(0, row, pattern[row]);
  }
}

void setup() {
  pinMode(ledBloqueoPin, OUTPUT);
  digitalWrite(ledBloqueoPin, LOW);
  lcd.begin(16, 2);
  lcd.clear();
  lcd.print("Iniciando...");
  delay(1000); 

  // Inicializa cada LED Matrix de piso
  for (int i = 0; i < 12; i++) {
    floorMatrices[i] = new LedControl(LED_DIN, LED_CLK, csPins[i], 1);
    floorMatrices[i]->shutdown(0, false);
    floorMatrices[i]->setIntensity(0, 8);
    floorMatrices[i]->clearDisplay(0);
  }
  
  // (Se asume que las matrices de flechas (matrixArriba y matrixAbajo) ya están conectadas y funcionan)
  matrixArriba.shutdown(0, false);
  matrixAbajo.shutdown(0, false);
  matrixArriba.setIntensity(0, 8);
  matrixAbajo.setIntensity(0, 8);
  pinMode(buzzerPin, OUTPUT);
  digitalWrite(buzzerPin, LOW);
  mostrarEstadoInicial();

}

void loop() {
  char tecla = keypad.getKey();  
  manejarTeclado(tecla);
  
  // Verificar si el bloqueo está activo
  if (bloqueado && (millis() - tiempoBloqueo) >= 10000) {
    bloqueado = false;
    digitalWrite(ledBloqueoPin, LOW);  // Apagar el LED de bloqueo
    abrirPuerta(); 
  }

  if (!bloqueado) {
    actualizarMovimiento();
    actualizarMatrices();
    actualizarLCD();
    actualizarFloorMatrices();
  }
}

void manejarTeclado(char tecla) {
  if(tecla) {
    switch(tecla) {
      case 'A': abrirPuerta(); break;
      case 'B': cerrarPuerta(); break;
      case 'C': toggleBloqueo(); break;
      case 'D': activarAlarma(); break;
      default:
        int piso = mapearTeclaAPiso(tecla);
        if(piso != -99 && piso != pisoActual) {
          agregarPisoDestino(piso);
        }
    }
  }
}

int mapearTeclaAPiso(char tecla) {
  switch(tecla) {
    case '1': return 1;
    case '2': return 2;
    case '3': return 3;
    case '4': return 4;
    case '5': return 5;
    case '6': return 6;
    case '7': return 7;
    case '8': return 8;
    case '9': return 9;
    case '0': return 0;
    case '*': return -1;
    case '#': return -2;
    default: return -99;
  }
}


void actualizarMovimiento() {
  if (!bloqueado && (millis() - tiempoMovimiento) > 2000 && !puertaAbierta) {
    bool pisoCambiado = false;
    if (pisoActual < destinoActual()) {
      direccion = 1;
      pisoActual++;
      pisoCambiado = true;
    } else if (pisoActual > destinoActual()) {
      direccion = -1;
      pisoActual--;
      pisoCambiado = true;
    } else {
      direccion = 0;
      abrirPuerta();
      eliminarPisoDestino(pisoActual);
    }
    
    if (pisoCambiado) {
      actualizarFloorMatrices();
    }
    tiempoMovimiento = millis();
  }
}


void actualizarFloorMatrices() {
  for (int i = 0; i < 12; i++) {
    // Si el módulo corresponde al piso actual (mapeado con offset 2)
    if (i == pisoActual + 2) {
      mostrarPatron(floorMatrices[i], floorPatterns[i]);
    } else {
      floorMatrices[i]->clearDisplay(0);
    }
  }
}


void actualizarMatrices() {
  if(direccion == 1) {
    dibujarFlecha(matrixArriba, arrowUp);
    matrixAbajo.clearDisplay(0);
  } else if(direccion == -1) {
    dibujarFlecha(matrixAbajo, arrowDown);
    matrixArriba.clearDisplay(0);
  } else {
    matrixArriba.clearDisplay(0);
    matrixAbajo.clearDisplay(0);
  }
}

void dibujarFlecha(LedControl &matrix, byte* flecha) {
  for(int i = 0; i < 8; i++) {
    matrix.setRow(0, i, flecha[i]);
  }
}

void actualizarLCD() {
  lcd.setCursor(0, 0);
  lcd.print("Piso: ");
  lcd.print(obtenerNombrePiso(pisoActual));
  lcd.print("  ");
  
  lcd.setCursor(0, 1);
  if(puertaAbierta) {
    lcd.print("Puerta Abierta  ");
  } else if(direccion == 1) {
    lcd.print("Subiendo...     ");
  } else if(direccion == -1) {
    lcd.print("Bajando...      ");
  } else {
    lcd.print("Detenido        ");
  }
}

String obtenerNombrePiso(int piso) {
  switch(piso) {
    case -2: return "SB2";
    case -1: return "SB1";
    case 0: return "PB0";
    default: return "P" + String(piso);
  }
}

void abrirPuerta() {
  puertaAbierta = true;
  lcd.setCursor(0, 1);
  lcd.print("Abriendo puerta ");
  delay(2000);
  lcd.setCursor(0, 1);
  lcd.print("Puerta Abierta  ");
}

void cerrarPuerta() {
  puertaAbierta = false;
  lcd.setCursor(0, 1);
  lcd.print("Cerrando puerta ");
  delay(2000);
  lcd.setCursor(0, 1);
  lcd.print("Puerta Cerrada  ");
}

void mostrarEstadoInicial() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Iniciando...");
  delay(1000);
}

void toggleBloqueo() {
  if (!bloqueado) {
    bloqueado = true;
    tiempoBloqueo = millis();  // Registrar el tiempo de inicio del bloqueo
    digitalWrite(ledBloqueoPin, HIGH);  // Encender el LED de bloqueo
    lcd.setCursor(0, 1);
    lcd.print("Bloqueado      ");
  }
}

void activarAlarma() {
  lcd.setCursor(0, 1);
  lcd.print("Alarma Activada!");
  
  // Enciende el buzzer
  digitalWrite(buzzerPin, HIGH);
  delay(5000); // Espera 5 segundos con el buzzer activo
  
  // Apaga el buzzer
  digitalWrite(buzzerPin, LOW);
  delay(1000); 
}


void agregarPisoDestino(int piso) {
  for (int i = 0; i < 10; i++) {
    if (listaDestinos[i] == 0) {
      listaDestinos[i] = piso;
      break;
    }
  }
}

void eliminarPisoDestino(int piso) {
  for (int i = 0; i < 10; i++) {
    if (listaDestinos[i] == piso) {
      listaDestinos[i] = 0;
      break;
    }
  }
}

int destinoActual() {
  for (int i = 0; i < 10; i++) {
    if (listaDestinos[i] != 0) {
      return listaDestinos[i];
    }
  }
  return pisoActual;
}
