#include <si5351.h>     
#include <Wire.h>       //I2C communication to SI5351a (A4=I2C Data, A5=I2C cl0ck)
#include <SoftwareSerial.h>
#include <SPI.h>
#include <LiquidCrystal_I2C.h>

LiquidCrystal_I2C lcd(0x27,16,2);

#define pin_1s_GPS  2 // D2 pin. 1s Signal from GPS 
#define pin_CLK0    5 // For info D5 is the pin dedicated to Timer 1 for the Nano


//Create si5531 device
//*******************

Si5351 si5351;
//SoftwareSerial GPSserial(4,3);


// Variables Xtal 25MHz
//*********************
int32_t Xtal_correction,Coef_correction; 
uint64_t Xtal_Freq,Oscil_1_Freq,Oscil_2_Freq; //Frequencies of the 3 Oscillators in 1/100 Hz

//Variables Timers 
//****************
unsigned long Overflow_Timer1_Counter=0;
unsigned int GPScount=0;
uint64_t Pulse_counted,Pulse_target;
int32_t  Pulse_diff,Pulse_counted4,Pulse_target40;

//Variables Communication
//***********************
int inByte = 0;
int zero=0;
char buffer[100] = "";
int counter = 0;
bool pps_flag=0;
//Variables Info
int32_t freq;



//SETUP PROGRAM
//*************

void setup() {
    Serial.begin(9600);  // Define Orange Pi port speed
    Serial.println("COM SERIAL OK");
    
    lcd.init();
    lcd.backlight();
    lcd.setCursor(0, 0);
    lcd.print("GPS DO");
    
    Serial.println("LCD DISPLAY OK");
    delay(200);

    // GPS 1pps input
    pinMode(pin_1s_GPS, INPUT);
    
    //Serial Port
    //GPSserial.begin(9600);
  
    //Serial.println("Hello,  Nano starts");
    //I2C Bus
    Wire.begin(1);    // I2C bus address = 1. Communication to si5531 

    //  Set up parameters of Xtal Oscillator
    si5351.init(SI5351_CRYSTAL_LOAD_8PF,0,0); // Medium value
    Xtal_correction = 0; //signed 32-bit integer of the * parts-per-billion value that the actual oscillation
                      // * frequency deviates from the specified frequency.
    Xtal_Freq=250000000ULL;    // In 1/100 Hz   2.5MHz instead of 25MHz
    Pulse_target=4*Xtal_Freq/10;  //100 Million in 40s
    Coef_correction = 5; // Correction coefficient to compensate counting error (half of what is needed)

    Pulse_target40=(uint32_t)(Pulse_target/40);
    //Serial.print("Pulse_target:");
    //Serial.println(String(Pulse_target40)+" Pulses/s");
    
    Oscil_1_Freq =  100000000ULL; 
    Oscil_2_Freq =  500000000ULL; 
    
    
    // Set up Si5351 calibration factor and frequencies
    si5351.drive_strength(SI5351_CLK0,SI5351_DRIVE_8MA); // define CLK0 output current
    si5351.drive_strength(SI5351_CLK1,SI5351_DRIVE_8MA); // define CLK1 output current 
    si5351.drive_strength(SI5351_CLK2,SI5351_DRIVE_8MA); // define CLK2 output current
    Set_Oscil();

    //Setup Timer1 as a frequency counter of si5351 Clock0 - input at pin D5
    //Timer-Counter-Control-Registers
    TCCR1B = 0;     //Disable Timer during setup
    TCCR1A = 0;     //Reset
    // Timer/Counter 1  on 16 bits
    TCNT1  = 0;     //Reset counter to zero
    Overflow_Timer1_Counter = 0;
    //Timer/Counter Interrupt Flag Register
    TIFR1  = 1;     //Reset overflow
    //Timer/Counter Interrupt Mask Register
    TIMSK1 = 1;     //Turn on overflow flag
    //GPS Pulse counter
    GPScount = 0;
   
    //Setup 1s interruption from the GPS
    attachInterrupt(digitalPinToInterrupt(pin_1s_GPS), GPSinterrupt, RISING) ;
  
}

//Reset function at address 0
void(* resetFunc) (void) = 0;

// LOOP PROGRAM
// ************
void loop(){
  
  Actualizar_LCD();
    
  if (GPScount >= 42) { //Timer 1 counter is stopped
        Pulse_counted4=(int32_t)(Pulse_counted/4);
        Serial.println("FreqXtalRef:"+String(Pulse_counted4) + " Hz");
        
        Pulse_diff=(int32_t)(Pulse_counted-Pulse_target);
        
        
        //Serial.println("PulseDiff:" +String(Pulse_diff) + " pulses/40s");
        if (Pulse_diff<1000000 && Pulse_diff>-1000000){ // Counting seems OK even it could be too important
          Xtal_correction=Xtal_correction + Pulse_diff*Coef_correction;
          if (Xtal_correction>4000000)  Xtal_correction=4000000; // Cannot be bigger
          if (Xtal_correction<-4000000)  Xtal_correction=-4000000;   // Max 100 kHz of error of the Xtal which is enormous
        } else {
          Serial.println("WARNING COUNTING PULSES ERROR");
        }
        Set_Oscil();
        GPScount = 0;      //Reset the seconds counter
  }

  //lectura de la trama GPS. Se envia al puerto serie.
  /* if (GPSserial.available() > 0) 
   {
    char data = GPSserial.read();
    Serial.print(data);
   }
  */       
}

// SUBROUTINES
// ***********

// Timer 1 overflow interupt vector.
ISR(TIMER1_OVF_vect) {
  Overflow_Timer1_Counter++;        //Increment overflow counter
  TIFR1 = (1<<TOV1);  //Clear overlow flag by shifting left 
}

void GPSinterrupt(){
  pps_flag=!pps_flag;
  Serial.print(".");
    GPScount++;// Increment GPS 1s pulse counter
  if (GPScount == 2) { // Start counting the 2.5 MHz signal from Si5351A CLK0
    TCCR1B = 7;    //Clock on rising edge of pin 5
  }
  if (GPScount == 42) { //Stop the counter : the 40 second gate time is elapsed   
    TCCR1B = 0;      //Turn off Timer 1 counter
    Pulse_counted = Overflow_Timer1_Counter * 0x10000 + TCNT1;   //Number of pulses counted during the 40s.
    //With a Xtal frequency is at  25 MHz and an input Clock 0 at 2.5MHz, Pulse_counted should be 100000000.    
    TCNT1 = 0;       //Reset counter to zero
    Overflow_Timer1_Counter = 0; 
  }
  
}
void Set_Oscil(){
    freq=(uint32_t)(Xtal_Freq/100);
    //Serial.println("FreqOscil_0:"+ String(freq)+" Hz");
 
    freq=(uint32_t)(Oscil_1_Freq/100);
    //Serial.println("FreqOscil_1:"+ String(freq)+" Hz");
    
    freq=(uint32_t)(Oscil_2_Freq/100);
    //Serial.println("FreqOscil_2:"+ String(freq)+" Hz");
    
    Serial.println("XtalCorrection:" +String(Xtal_correction)+" ppb");
    
    si5351.set_correction(Xtal_correction, SI5351_PLL_INPUT_XO); 
    si5351.set_freq(Xtal_Freq, SI5351_CLK0);
    si5351.set_freq(Oscil_1_Freq, SI5351_CLK1); 
    si5351.set_freq(Oscil_2_Freq, SI5351_CLK2); 
}
 void Actualizar_LCD()
 {
  lcd.setCursor(0, 0);
  lcd.print("GPS DO          ");
  lcd.setCursor(12, 0);
  if (pps_flag==1)
  {
    lcd.print("1pps");
  }
  else
  {
    lcd.print("    ");
  }
  lcd.setCursor(0, 1);
  lcd.println("Error: "+String(Pulse_diff/100.00)+" ppm");     
 }
