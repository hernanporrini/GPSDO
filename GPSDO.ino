#include <si5351.h>     
#include <Wire.h>             //I2C comunicacion con SI535a (A4=Data, A5=clock)
#include <SPI.h>
#include <LiquidCrystal_I2C.h>

LiquidCrystal_I2C lcd(0x27,16,2);
Si5351 si5351;

#define     pin_1pps    2 // D2 pin de entrada de la señal 1PPS del GPS 
#define     pin_ch0     5 // D5 pin del Timer 1 donde contara el lazo cerrado del clock0

int32_t     Xtal_correccion = 0;
int32_t     Coef_correccion = 5; // coeficiente de Correction para compensate el error de cuenta 
uint64_t    Frec_ch0 =   100000000ULL; //Frecuencias del canal 1 del si5351 en 1/100 Hz
uint64_t    Frec_ch1 =  1000000000ULL; //Frecuencias del canal 2 del si5351 en 1/100 Hz
uint64_t    Frec_ch2 = 10000000000ULL; //Frecuencias del canal 3 del si5351 en 1/100 Hz
unsigned long   Overflow_Timer1_Contador=0;
unsigned int    GPS_contador_PPS=0;
float       tiempo = 40.00;
uint64_t    Pulsos_contados;
uint64_t    Pulsos_esperados = 40000000;  //tiene que ser 1,000,000 X tiempo
int32_t     Pulsos_diferencia;


void setup() 
    {
    Serial.begin(9600);  
    Serial.println("COM SERIAL OK");
    lcd.init();
    lcd.backlight();
    lcd.setCursor(0, 0);
    lcd.print("->>> GPS.DO <<<-");
    lcd.setCursor(0, 1);
    lcd.print("Hernan LW1EHP v1");
    delay(5000);
    lcd.setCursor(0, 0);
    lcd.print("GPS DO          ");
    lcd.setCursor(0, 1);
    lcd.println("Error: ---.--ppm");     
    Serial.println("LCD DISPLAY OK");
    pinMode(pin_1pps, INPUT);
    Wire.begin(1);    // I2C bus address = 1. Comunicacion con el si5531 
    si5351.init(SI5351_CRYSTAL_LOAD_8PF,0,0); // por defaiult se utilizan capacitores internos de 8pF
    si5351.drive_strength(SI5351_CLK0,SI5351_DRIVE_8MA); // se define la corriente de salida del canal 0
    si5351.drive_strength(SI5351_CLK1,SI5351_DRIVE_8MA); // se define la corriente de salida del canal 1
    si5351.drive_strength(SI5351_CLK2,SI5351_DRIVE_8MA); // se define la corriente de salida del canal 2
    Set_Oscil();
    si5351.set_correction(Xtal_correccion, SI5351_PLL_INPUT_XO); 
    si5351.set_freq(Frec_ch0, SI5351_CLK0); // se define la frecuencia de salida del canal 0
    si5351.set_freq(Frec_ch1, SI5351_CLK1); // se define la frecuencia de salida del canal 1
    si5351.set_freq(Frec_ch2, SI5351_CLK2); // se define la frecuencia de salida del canal 2
    //Seteo del Timer1 como un frecuencimetro del canal 0 del si5351 en el pin D5
    TCCR1B = 0;     
    TCCR1A = 0;     
    TCNT1  = 0;     
    Overflow_Timer1_Contador = 0;
    TIFR1  = 1;     
    TIMSK1 = 1;     
    attachInterrupt(digitalPinToInterrupt(pin_1pps), GPSinterrupcion, RISING) ;
    lcd.setCursor(0, 0);
    lcd.print("->GPS.DO<-  ");
    }

void(* resetFunc) (void) = 0;

void loop()
    {
    Actualizar_pps_LCD();
    if (GPS_contador_PPS >= (int(tiempo)+2)) 
        { 
        Pulsos_diferencia=(int32_t)(Pulsos_contados-Pulsos_esperados);
        if (Pulsos_diferencia<1000000 && Pulsos_diferencia>-1000000)
            { 
            Xtal_correccion=Xtal_correccion + Pulsos_diferencia*Coef_correccion;
            if (Xtal_correccion>4000000)   Xtal_correccion= 4000000; 
            if (Xtal_correccion<-4000000)  Xtal_correccion=-4000000;
            }
        si5351.set_correction(Xtal_correccion, SI5351_PLL_INPUT_XO); 
        GPS_contador_PPS = 0;      
        Actualizar_error_LCD();
        }
    }

ISR(TIMER1_OVF_vect)                  
    {
    Overflow_Timer1_Contador++;       
    TIFR1 = (1<<TOV1);                
    }

void GPSinterrupcion()
    {
    pps_flag=!pps_flag;
    //Serial.print(".");
    GPS_contador_PPS++;             // Incremento el contado de la señal 1PPS del GPS
    if (GPS_contador_PPS == 2) 
        {                           // Comienzo a contar los pulsos del canal0 en el pin D5
        TCCR1B = 7;                 
        }
    if (GPS_contador_PPS == (int(tiempo)+2)) 
        {                           
        TCCR1B = 0;             // Detengo el contador, tras el termino de la ventana de TIEMPO definida    
        Pulsos_contados = Overflow_Timer1_Contador * 0x10000 + TCNT1;   // Numero de pulsos contados
        TCNT1 = 0;                                              // Reseteo el contador a cero
        Overflow_Timer1_Contador = 0; 
        }
    }

void Actualizar_error_LCD()
    {
    lcd.setCursor(0, 1);
    if (Pulsos_diferencia<0)    lcd.println("Error:" +String(Pulsos_diferencia/tiempo)+"ppm        ");    
    else                        lcd.println("Error: "+String(Pulsos_diferencia/tiempo)+"ppm        ");     
    }

void Actualizar_pps_LCD()
    {
    lcd.setCursor(12, 0);
    if (pps_flag==1)  lcd.print("1pps");  
    else              lcd.print("    ");
    }
