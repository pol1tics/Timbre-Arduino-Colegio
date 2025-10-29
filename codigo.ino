// =================================================================================
// CÓDIGO FINAL: Timbre Automático Programable para Colegio
// =================================================================================

#include <Wire.h> 
#include "RTClib.h"
#include <LiquidCrystal_I2C.h>

// ---------------------
// 1. DEFINICIONES GLOBALES Y PINES
// ---------------------

const uint8_t PIN_TIMBRE = 7;           // Pin digital para el relé del timbre/campana
const uint16_t UMBRAL_CONTACTO = 800;   // Umbral de lectura analógica para considerar un contacto "ON"

#define MAX_TIMBRES 16
#define HORA_NO_PROGRAMADA 99

// DURACIONES DEL TIMBRE
#define DURACION_LARGA 5000  // 5 segundos (Entrada, Salida, Reingresos Críticos)
#define DURACION_CORTA 3000  // 3 segundos (Cambios de Clase estándar)

// CONFIGURACIÓN DE OPERACIÓN Y FALLAS
const uint8_t HORA_INICIO_OPERACION = 6;  // 6:00 AM
const uint8_t HORA_FIN_OPERACION = 22;    // 7:00 PM
const uint16_t UMBRAL_FALLA_CONTACTO = 10; // Lectura mínima para considerar un cable desconectado

// ** MODO DE PRUEBA (TEST MODE) **
// Cambiar a 'false' para activar el relé físico
bool MODO_PRUEBA_ACTIVO = true; 

// Estructura para festivos (Día, Mes)
struct Festivo {
  uint8_t dia;
  uint8_t mes;
};

// Lista de Festivos (Ejemplo de festivos colombianos)
const Festivo listaFestivos[] = {
  {1, 1}, {6, 1}, {23, 3}, {2, 4}, {3, 4}, {1, 5}, 
  {18, 5}, {8, 6}, {15, 6}, {29, 6}, {20, 7}, {7, 8}, 
  {17, 8}, {12, 10}, {2, 11}, {16, 11}, {8, 12}, {25, 12}   
};
const size_t NUM_FESTIVOS = sizeof(listaFestivos) / sizeof(listaFestivos[0]);

// ESTRUCTURA DE TIMBRE
struct Timbre {
  uint8_t hora;
  uint8_t minuto;
  uint8_t segundo;
  uint16_t duracion_ms; // Duración variable del toque
};

// Variables Globales
LiquidCrystal_I2C lcd(0x27, 16, 2); // Ajusta la dirección I2C si es necesario (0x27 o 0x3F)
RTC_DS1307 RTC;
uint8_t segundo, minuto, hora; 

// ---------------------
// 2. ARREGLOS DE HORARIOS PROGRAMADOS
// ---------------------

// CONTACTO 1 (A0) - HORARIO LUNES A JUEVES
const Timbre horario1[MAX_TIMBRES] = {
  // Hora, Minuto, Segundo, Duración (ms)
  {7, 0, 0, DURACION_LARGA},   // 7:00 (Entrada) -> 5s
  {8, 0, 0, DURACION_CORTA},   // 8:00 (Cambio) -> 3s
  {9, 0, 0, DURACION_CORTA},   // 9:00 (Cambio) -> 3s
  {10, 0, 0, DURACION_LARGA},  // 10:00 (Inicio Pausa) -> 5s
  {10, 20, 0, DURACION_LARGA}, // 10:20 (Reingreso Pausa) -> 5s
  {11, 20, 0, DURACION_CORTA}, // 11:20 (Cambio) -> 3s
  {12, 20, 0, DURACION_LARGA}, // 12:20 (Inicio Almuerzo) -> 5s
  {13, 0, 0, DURACION_LARGA},  // 13:00 (Reingreso Almuerzo) -> 5s
  {14, 0, 0, DURACION_CORTA},  // 14:00 (Cambio) -> 3s
  {15, 0, 0, DURACION_LARGA},  // 15:00 (Salida) -> 5s
  // Relleno
  {HORA_NO_PROGRAMADA, 0, 0, 0}, {HORA_NO_PROGRAMADA, 0, 0, 0}, {HORA_NO_PROGRAMADA, 0, 0, 0},
  {HORA_NO_PROGRAMADA, 0, 0, 0}, {HORA_NO_PROGRAMADA, 0, 0, 0}, {HORA_NO_PROGRAMADA, 0, 0, 0}
};

// CONTACTO 2 (A1) - HORARIO VIERNES
const Timbre horario2[MAX_TIMBRES] = {
  // Hora, Minuto, Segundo, Duración (ms)
  {7, 0, 0, DURACION_LARGA},   // 7:00 (Entrada) -> 5s
  {7, 50, 0, DURACION_CORTA},   // 7:50 (Cambio) -> 3s
  {8, 40, 0, DURACION_CORTA},   // 8:40 (Cambio) -> 3s
  {9, 30, 0, DURACION_LARGA},  // 9:30 (Inicio Pausa) -> 5s
  {10, 20, 0, DURACION_LARGA}, // 10:20 (Reingreso Pausa) -> 5s
  {10, 40, 0, DURACION_CORTA}, // 10:40 (Cambio) -> 3s
  {11, 30, 0, DURACION_CORTA}, // 11:30 (Cambio, previo al almuerzo) -> 3s
  {12, 20, 0, DURACION_LARGA}, // 12:20 (Inicio Almuerzo) -> 5s
  {13, 0, 0, DURACION_LARGA},  // 13:00 (Reingreso Almuerzo) -> 5s
  {14, 0, 0, DURACION_LARGA},  // 14:00 (Salida) -> 5s
  // Relleno
  {HORA_NO_PROGRAMADA, 0, 0, 0}, {HORA_NO_PROGRAMADA, 0, 0, 0}, {HORA_NO_PROGRAMADA, 0, 0, 0},
  {HORA_NO_PROGRAMADA, 0, 0, 0}, {HORA_NO_PROGRAMADA, 0, 0, 0}, {HORA_NO_PROGRAMADA, 0, 0, 0}
};

// CONTACTO 3 (A2) - HORARIO MANUAL / EVENTOS (Se mantiene el horario anterior como opción)
const Timbre horario3[MAX_TIMBRES] = {
  {7, 0, 0, DURACION_LARGA}, {8, 30, 0, DURACION_CORTA}, {9, 0, 0, DURACION_CORTA}, {9, 30, 0, DURACION_CORTA},
  {9, 45, 0, DURACION_LARGA}, {10, 15, 0, DURACION_CORTA}, {10, 45, 0, DURACION_CORTA}, {11, 0, 0, DURACION_CORTA},
  {11, 30, 0, DURACION_CORTA}, {12, 0, 0, DURACION_LARGA},
  {HORA_NO_PROGRAMADA, 0, 0, 0}, {HORA_NO_PROGRAMADA, 0, 0, 0}, {HORA_NO_PROGRAMADA, 0, 0, 0},
  {HORA_NO_PROGRAMADA, 0, 0, 0}, {HORA_NO_PROGRAMADA, 0, 0, 0}, {HORA_NO_PROGRAMADA, 0, 0, 0}
};


// ---------------------
// 3. DECLARACIÓN DE FUNCIONES AUXILIARES
// ---------------------

void activar_timbre(uint16_t duracion_ms); 
void verificar_horario(const Timbre *horarios, int id_horario);
void mostrar_dia_semana(uint8_t diaSemana);
void imprimir_estado_serial(DateTime now, uint8_t diaSemana, bool timbre_activo_finde, int contacto1, int contacto2, int contacto3, int contacto4);
bool es_dia_festivo(DateTime now);
void procesar_comando_serial();
bool es_horario_operativo(uint8_t h, uint8_t m);
bool verificar_fallas_contacto(int c1, int c2, int c3, int c4);
bool manejar_alerta_falla(int c1, int c2, int c3, int c4);

// =================================
// 4. VOID SETUP()
// =================================
void setup() {
  pinMode(PIN_TIMBRE, OUTPUT);
  digitalWrite(PIN_TIMBRE, LOW); 

  Wire.begin();
  RTC.begin();

  Serial.begin(9600);
  Serial.println("==================================");
  Serial.println("--- Timbre Automatico Iniciado ---");
  Serial.print(">>> MODO PRUEBA ACTIVO: "); Serial.println(MODO_PRUEBA_ACTIVO ? "SI" : "NO");
  Serial.println("==================================");
  Serial.println("Para actualizar la hora, use el formato:");
  Serial.println("SETTIME=YYYY/MM/DD/hh/mm/ss");
  
  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.print("Timbre Automatico");
}

// =================================
// 5. VOID LOOP() - LÓGICA PRINCIPAL
// =================================
void loop() {
  
  procesar_comando_serial();

  DateTime now = RTC.now(); 
  uint8_t diaSemana = now.dayOfTheWeek(); // 0=Domingo, 1=Lunes, ..., 5=Viernes, 6=Sábado

  segundo = now.second();
  minuto = now.minute();
  hora = now.hour();

  // Lectura de Contactos Analógicos
  int contacto1 = analogRead(A0); 
  int contacto2 = analogRead(A1); 
  int contacto3 = analogRead(A2); 
  int contacto4 = analogRead(A3); // Pin de control de fin de semana

  // ---------------------------------------------
  // AUTODIAGNÓSTICO Y ALERTA DE FALLA
  if (!MODO_PRUEBA_ACTIVO) {
    if (manejar_alerta_falla(contacto1, contacto2, contacto3, contacto4)) {
      return; // Detiene el loop si hay una falla de contacto grave
    }
  } 
  // ---------------------------------------------
  
  bool fin_semana = (diaSemana == 6 || diaSemana == 0);
  bool timbre_activo_en_finde = (contacto4 >= UMBRAL_CONTACTO);

  // 1. MODO NOCTURNO
  if (!es_horario_operativo(hora, minuto)) {
    lcd.setCursor(0, 0); lcd.print("MODO NOCTURNO ON"); 
    lcd.setCursor(15, 0); lcd.print("N");
    digitalWrite(PIN_TIMBRE, LOW); 
    delay(500); 
    return;
  }

  // 2. DÍA FESTIVO
  if (es_dia_festivo(now)) {
    lcd.setCursor(0, 0); lcd.print("DIA FESTIVO ON  "); 
    lcd.setCursor(15, 0); lcd.print("F");
    digitalWrite(PIN_TIMBRE, LOW); 
    delay(500); 
    return;
  }

  imprimir_estado_serial(now, diaSemana, timbre_activo_en_finde, contacto1, contacto2, contacto3, contacto4);

  // 3. IMPRESIÓN EN LCD 
  lcd.setCursor(0, 0); lcd.print("                "); 
  lcd.setCursor(0, 0);
  lcd.print("D:");
  lcd.print(now.day(), DEC); lcd.print("/"); lcd.print(now.month(), DEC); lcd.print("/"); lcd.print(now.year(), DEC);

  lcd.setCursor(12, 0);
  if (MODO_PRUEBA_ACTIVO) {
      lcd.print("T"); // 'T' para Test Mode
  } else {
      timbre_activo_en_finde ? lcd.print("F") : lcd.print("e"); // 'F' para Fin de Semana Activo, 'e' para Silencio
  }

  lcd.setCursor(0, 1);
  lcd.print("T: ");
  lcd.print(now.hour(), DEC); lcd.print(":"); lcd.print(now.minute(), DEC); lcd.print(":"); lcd.print(now.second(), DEC);
  
  mostrar_dia_semana(diaSemana);
  
  // 4. LÓGICA DE ACTIVACIÓN DEL TIMBRE
  if (fin_semana && !timbre_activo_en_finde) {
    // Si es fin de semana Y el control A3 está desactivado, el timbre se pausa.
  } else {
    
    // a) SELECCIÓN AUTOMÁTICA (A0 y A1)
    if (contacto1 >= UMBRAL_CONTACTO || contacto2 >= UMBRAL_CONTACTO) {
        
        if (diaSemana >= 1 && diaSemana <= 4 && contacto1 >= UMBRAL_CONTACTO) { // Lunes a Jueves
            verificar_horario(horario1, 1);
        } else if (diaSemana == 5 && contacto2 >= UMBRAL_CONTACTO) { // Viernes
            verificar_horario(horario2, 2);
        }
    } 
    
    // b) HORARIO MANUAL (A2) - Siempre se ejecuta si está activo
    if (contacto3 >= UMBRAL_CONTACTO) {
      Serial.println("SELECCION MANUAL: HORARIO 3 (A2)");
      verificar_horario(horario3, 3);
    }
  }

  // Asegura que el pin esté LOW al final del ciclo (solo en modo normal)
  if (!MODO_PRUEBA_ACTIVO) {
    digitalWrite(PIN_TIMBRE, LOW);
  }
  
  delay(500); // Pausa para reducir el consumo de CPU y asegurar la verificación por segundo
}

// =================================
// 6. IMPLEMENTACIÓN DE FUNCIONES
// =================================

/**
 * @brief Activa el relé por el tiempo definido (duracion_ms).
 */
void activar_timbre(uint16_t duracion_ms) {
  
  if (MODO_PRUEBA_ACTIVO) {
    // Modo de prueba: Solo simula la activación
    lcd.setCursor(0, 0); lcd.print("[TEST] SONARIA!  "); 
    Serial.print(">>> [TEST MODE] TIMBRE HUBIERA SIDO ACTIVADO (");
    Serial.print(duracion_ms); Serial.println("ms) <<<");
    delay(duracion_ms); 
  } else {
    // Modo normal: Activa el pin físico
    digitalWrite(PIN_TIMBRE, HIGH);
    lcd.setCursor(0, 0); lcd.print("TIMBRE ON!       "); 
    Serial.print(">>> TIMBRE ACTIVADO (");
    Serial.print(duracion_ms); Serial.println("ms) <<<");
    delay(duracion_ms); 
    digitalWrite(PIN_TIMBRE, LOW); 
  }
}

/**
 * @brief Itera sobre un arreglo de horarios para comprobar si coincide con la hora actual.
 */
void verificar_horario(const Timbre *horarios, int id_horario) {
  lcd.setCursor(15, 0); 
  lcd.print(id_horario);

  for (int i = 0; i < MAX_TIMBRES; i++) {
    if (horarios[i].hora == HORA_NO_PROGRAMADA) {
      break;
    }

    if (hora == horarios[i].hora && minuto == horarios[i].minuto && segundo == horarios[i].segundo) {
      activar_timbre(horarios[i].duracion_ms); 
      return;
    }
  }
}

/**
 * @brief Muestra el día de la semana en el LCD.
 */
void mostrar_dia_semana(uint8_t diaSemana) {
  lcd.setCursor(12, 1);

  switch (diaSemana) {
    case 1: lcd.print("Lun"); break;
    case 2: lcd.print("Mar"); break;
    case 3: lcd.print("Mie"); break;
    case 4: lcd.print("Jue"); break;
    case 5: lcd.print("Vie"); break;
    case 6: lcd.print("Sab"); break;
    case 0: lcd.print("Dom"); break;
    default: lcd.print("---"); break;
  }
}

/**
 * @brief Imprime la información de estado en el Monitor Serie.
 */
void imprimir_estado_serial(DateTime now, uint8_t diaSemana, bool timbre_activo_finde, int c1, int c2, int c3, int c4) {
    const char *dias[] = {"Dom", "Lun", "Mar", "Mie", "Jue", "Vie", "Sab"};
    
    Serial.println("----------------------------------");
    
    Serial.print("FECHA/HORA: ");
    Serial.print(now.day()); Serial.print("/"); Serial.print(now.month()); 
    Serial.print("/"); Serial.print(now.year()); Serial.print(" | ");
    Serial.print(now.hour() < 10 ? "0" : ""); Serial.print(now.hour()); Serial.print(":");
    Serial.print(now.minute() < 10 ? "0" : ""); Serial.print(now.minute()); Serial.print(":");
    Serial.print(now.second() < 10 ? "0" : ""); Serial.println(now.second());

    Serial.print("DIA SEMANA: "); Serial.print(dias[diaSemana]);
    Serial.print(" | FINDE C4: ");
    Serial.println(timbre_activo_finde ? "ACTIVO (F)" : "INACTIVO (e)");
    
    Serial.print("CONTACTOS (A0-A3): ");
    Serial.print("H1:"); Serial.print(c1); 
    Serial.print(" H2:"); Serial.print(c2); 
    Serial.print(" H3:"); Serial.print(c3);
    Serial.print(" C4:"); Serial.println(c4);
}

/**
 * @brief Verifica si la fecha actual es un día festivo programado.
 */
bool es_dia_festivo(DateTime now) {
  for (size_t i = 0; i < NUM_FESTIVOS; i++) {
    if (now.day() == listaFestivos[i].dia && now.month() == listaFestivos[i].mes) {
      return true;
    }
  }
  return false;
}

/**
 * @brief Verifica si la hora actual está dentro del rango operativo (Modo Nocturno).
 */
bool es_horario_operativo(uint8_t h, uint8_t m) {
    uint16_t tiempo_actual = h * 60 + m; 
    uint16_t inicio_operacion = HORA_INICIO_OPERACION * 60;
    uint16_t fin_operacion = HORA_FIN_OPERACION * 60;

    return (tiempo_actual >= inicio_operacion && tiempo_actual < fin_operacion);
}

/**
 * @brief Verifica si las lecturas de los contactos son peligrosamente bajas.
 */
bool verificar_fallas_contacto(int c1, int c2, int c3, int c4) {
  int contactos[] = {c1, c2, c3, c4};
  bool hay_falla = false;

  for (int i = 0; i < 4; i++) {
    if (contactos[i] < UMBRAL_FALLA_CONTACTO) {
      hay_falla = true;
      if (!MODO_PRUEBA_ACTIVO) {
          Serial.print("!!! ALERTA DE FALLA !!! Contacto A"); Serial.print(i);
          Serial.print(" fallando. Lectura: "); Serial.println(contactos[i]);
      }
    }
  }
  return hay_falla;
}

/**
 * @brief Maneja la alerta visual y sonora de falla de contacto.
 */
bool manejar_alerta_falla(int c1, int c2, int c3, int c4) {
  
  if (verificar_fallas_contacto(c1, c2, c3, c4)) {
    lcd.setCursor(0, 0); lcd.print("!!! FALLA CONTACTO !!!");
    
    // Alerta sonora (solo en modo normal)
    if (!MODO_PRUEBA_ACTIVO) {
        digitalWrite(PIN_TIMBRE, HIGH); 
        delay(200); 
        digitalWrite(PIN_TIMBRE, LOW); 
    }
    
    delay(1000); 
    return true; 
  }
  return false; 
}

/**
 * @brief Lee y procesa el comando SETTIME del Monitor Serie para ajustar el RTC.
 */
void procesar_comando_serial() {
  if (Serial.available()) {
    String comando = Serial.readStringUntil('\n');
    comando.trim(); 
    
    if (comando.startsWith("SETTIME=")) {
      String datos = comando.substring(8); 
      
      if (datos.length() >= 19) {
          int year = datos.substring(0, 4).toInt();
          int month = datos.substring(5, 7).toInt();
          int day = datos.substring(8, 10).toInt();
          int hour = datos.substring(11, 13).toInt();
          int minute = datos.substring(14, 16).toInt();
          int second = datos.substring(17, 19).toInt();
          
          if (year >= 2000 && month >= 1 && month <= 12 && day >= 1 && day <= 31 && hour >= 0 && hour <= 23 && minute >= 0 && minute <= 59 && second >= 0 && second <= 59) {
            DateTime nuevaHora(year, month, day, hour, minute, second);
            RTC.adjust(nuevaHora);
            
            Serial.println(">>> RTC ACTUALIZADO CON EXITO <<<");
            lcd.clear();
            lcd.print("Hora Actualizada!");
            lcd.setCursor(0, 1);
            lcd.print("RTC OK");
            delay(2000); 
            lcd.clear(); 
          } else {
            Serial.println("!!! ERROR: Valores de fecha/hora fuera de rango. Verifique el formato YYYY/MM/DD/hh/mm/ss !!!");
          }
      } else {
         Serial.println("!!! ERROR: Comando incompleto. Use SETTIME=YYYY/MM/DD/hh/mm/ss !!!");
      }
    }
  }
}