#include <Adafruit_NeoPixel.h>

Adafruit_NeoPixel pixels = Adafruit_NeoPixel(95,12, NEO_GRB + NEO_KHZ800);


#include <driver/i2s.h>
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
#include <esp_task_wdt.h>

#include "sonidos.h"


#include <Wire.h>


#define MPU 0x68

TaskHandle_t Task1;

int16_t ax=10;
int16_t ay=20;
int16_t az=30;
int16_t gx=0;
int16_t gy=0;
int16_t gz=0;
int16_t temp=25;

float afgx=0;
float afgy=0;
float afgz=0;

float fgx=0;
float fgy=0;
float fgz=0;

int dgx=0;
int dgy=0;
int dgz=0;

//GPIO's
//#define BCK  26
//#define LRC  27
//#define DOU  25


// Configuración del bus I2S PCM5102
i2s_config_t i2sConfig;

// Buffer de salida de audio
int16_t audioBuffer[512];
size_t bytesWritten;

boolean encendido=false;

int tiempo;

int color=0;

int tiempochoque=0;

int tiemposonido=0;

boolean spositivo=true;
int eje=0;
boolean choque=false;

const int umbralchoque=2000;

void detectachoque(){
    int tiempochoque=millis();
    choque=true;
    
    for(;;){
      
        leempu6050();   
        
        if((eje==0)&&(spositivo)&&(dgx<-umbralchoque)){ break; }
        else if((eje==0)&&(!spositivo)&&(dgx>umbralchoque)){ break; }
        else if((eje==1)&&(spositivo)&&(dgy<-umbralchoque)){ break; }
        else if((eje==1)&&(!spositivo)&&(dgy>umbralchoque)){ break; }
        else if((eje==2)&&(spositivo)&&(dgz<-umbralchoque)){ break; }
        else if((eje==2)&&(!spositivo)&&(dgz>umbralchoque)){ break; }
                
        if((millis()-tiempochoque)>30){ choque=false; break; }
        delay(10);
    }    
  
}

void yield2(){

   esp_task_wdt_init(30,false);
   esp_task_wdt_reset();
   yield();
   
}

void leempu6050() {

  Wire.beginTransmission(MPU);
  Wire.write(0x3B); //Pedir el registro 0x3B - corresponde al AcX
  Wire.endTransmission(false);
  Wire.requestFrom(MPU,14,true); //A partir del 0x3B, se piden 6 registros
  while(Wire.available() < 14);
  
  ax=Wire.read()<<8|Wire.read(); //Cada valor ocupa 2 registros
  ay=Wire.read()<<8|Wire.read();
  az=Wire.read()<<8|Wire.read();
  
  temp=Wire.read()<<8|Wire.read();

  afgx=fgx;afgy=fgy;afgz=fgz;
  
  gx=Wire.read()<<8|Wire.read(); //Cada valor ocupa 2 registros
  gy=Wire.read()<<8|Wire.read();
  gz=Wire.read()<<8|Wire.read();

  fgx=((0.8*(float)fgx)+(0.2*(float)gx));
  fgy=((0.8*(float)fgy)+(0.2*(float)gy));
  fgz=((0.8*(float)fgz)+(0.2*(float)gz));  

  dgx=fgx-afgx;
  dgy=fgy-afgy;
  dgz=fgz-afgz;
        
}

void silencio(int dato){

  if(dato==0){ return; }
  
  int j=0;  

  boolean positivo=true;

  if(dato<0){ positivo=false; }  
      
  for(;;){

    if(positivo) { dato-=128;if(dato<0){dato=0;} }
    else{ dato+=128; if(dato>0){dato=0;} }
        
    audioBuffer[j]=dato;
    audioBuffer[j+1]=dato;      
        
    j+=2;
    
    if(j>=sizeof(audioBuffer)/sizeof(int16_t)){                 

      //Serial.println(audioBuffer[j-2]);
      j=0;            
      i2s_write(I2S_NUM_0, audioBuffer, sizeof(audioBuffer), &bytesWritten, portMAX_DELAY);

      //delay(1);
      yield2();        
      if(dato==0) { return; }
    }       
  }

   
}

void reproduce(int sonido){
 
  int j=0;  
  int i=0;

  int16_t dato=0;
  
  boolean termino=false;
  
  for(;;){   

    if(termino){ dato=dato/2; }
    else{
      
      if(sonido==0){  dato = espadaon[i+1]<<8 | espadaon[i]; }
      else if(sonido==1){  dato = espadaoff[i+1]<<8 | espadaoff[i]; }
      else if(sonido==2){  dato = espadamove1[i+1]<<8 | espadamove1[i]; }
      else if(sonido==3){  dato = espadamove2[i+1]<<8 | espadamove2[i]; }
      else if(sonido==4){  dato = espadamove3[i+1]<<8 | espadamove3[i]; }
      else if(sonido==5){  dato = espadachoque[i+1]<<8 | espadachoque[i]; }      
    }

    audioBuffer[j]=dato;
    audioBuffer[j+1]=dato;       
        
    j+=2;

    i+=2;
        
    if(j>=sizeof(audioBuffer)/sizeof(int16_t)){                 
      
      //Serial.println(audioBuffer[j-2]);
      j=0;            
      i2s_write(I2S_NUM_0, audioBuffer, sizeof(audioBuffer), &bytesWritten, portMAX_DELAY);

      yield2();  
      if(termino) { break; }      
    }   
    
    if((i>=espadaontam)&&(sonido==0)) { termino=true; }
    else if((i>=espadaofftam)&&(sonido==1)) { termino=true; }
    else if((i>=espadamove1tam)&&(sonido==2)) { termino=true; }
    else if((i>=espadamove2tam)&&(sonido==3)) { termino=true; }
    else if((i>=espadamove3tam)&&(sonido==4)) { termino=true; }
    else if((i>=espadachoquetam)&&(sonido==5)) { termino=true; }
    
  }

  silencio(dato);

  tiemposonido=millis();
  
 // Serial.println("Sale de reproducir");
  
}


void setup() {

  
  Serial.begin(115200);
  pixels.begin();

  int num=sizeof(audioBuffer)/sizeof(int16_t);
  Serial.println(num);

  
  
  pinMode(36,INPUT_PULLUP);
  
  Wire.begin(21,22);
  Wire.beginTransmission(MPU);
  Wire.write(0x6B);
  Wire.write(0);
  Wire.endTransmission(true);
  
  Wire.beginTransmission(MPU);
  Wire.write(0x19);   //Sample Rate
  Wire.write(0x00);   //  8000 / 1 + 0
  Wire.endTransmission(true);
  
  Wire.beginTransmission(MPU);
  Wire.write(0x1C);   //Setting Accel
  Wire.write(0x18);      //  + / - 16g
  Wire.endTransmission(true);
   
  
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);
    
  
  // Configurar la configuración del bus I2S  PCM5102
  
  i2sConfig.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX);
  i2sConfig.sample_rate = 44100;
  i2sConfig.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT;
  i2sConfig.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT;
  i2sConfig.communication_format = i2s_comm_format_t(I2S_COMM_FORMAT_I2S | I2S_COMM_FORMAT_I2S_MSB);
  i2sConfig.intr_alloc_flags = 0;
  i2sConfig.dma_buf_count = 8;
  i2sConfig.dma_buf_len = 256;
  i2sConfig.use_apll = false;
  i2sConfig.tx_desc_auto_clear = true;
  i2sConfig.fixed_mclk = 0;
  //i2sConfig.clkm_div_num = 1;

   // Inicializar el bus I2S con la configuración
  i2s_driver_install(I2S_NUM_0, &i2sConfig, 0, NULL);

  // Configurar el pinout del bus I2S
  i2s_pin_config_t pinConfig;
  pinConfig.bck_io_num = 26;    // Pin de reloj de bit
  pinConfig.ws_io_num = 27;     // Pin de selección de palabra
  pinConfig.data_out_num = 25;  // Pin de datos de salida
  pinConfig.data_in_num = -1;   // No se utiliza la entrada de datos
  i2s_set_pin(I2S_NUM_0, &pinConfig);

  /*
  for(int i=0;i<5;i++){
    reproduce(0);delay(500);
    reproduce(1);delay(500);
    reproduce(2);delay(500);
    reproduce(3);delay(500);
    reproduce(4);delay(500);
    reproduce(5);delay(500);   
  }
  */
  
   xTaskCreatePinnedToCore(
                    Task1code,   /* Task function. */
                    "Task1",     /* name of task. */
                    10000,       /* Stack size of task */
                    NULL,        /* parameter of the task */
                    1,           /* priority of the task */
                    &Task1,      /* Task handle to keep track of created task */
                    0);          /* pin task to core 0 */ 

  tiempo=millis();
  
  tiemposonido=millis();
     
}


void Task1code( void * pvParameters ){    // en este Core recogemos las peticiones web

  int r=0;
  int g=0;
  int b=100;
    
  for(;;){   
    
    while(!encendido) { 
      if(color==0){ r=0;g=0;b=40; }        
      else if(color==1){ r=40;g=0;b=0; }        
      else if(color==2){ r=0;g=40;b=0; }        
      else if(color==3){ r=20;g=0;b=20; }        
      else if(color==4){ r=20;g=20;b=0; }        
      else if(color==5){ r=0;g=20;b=20; }        
      
      pixels.setPixelColor(0,pixels.Color(r,g,b)); 
      delay(10);  
      pixels.show();  

      yield2();       
    }
         
    for(int i=0;i<95;i++){          
      pixels.setPixelColor(i,pixels.Color(r,g,b));   
      delay(5);    
      pixels.show();
      yield2();         
    }    

    while(encendido) { delay(10); yield2();  }
    
    for(int i=95;i>=0;i--){
      pixels.setPixelColor(i,pixels.Color(0,0,0));      
      delay(5);
      //Serial.print(".");
      pixels.show(); 
      yield2();     
    }
        
  } 
}

void loop() {    

   // if(encendido){ Serial.println("encendido"); }
   // else{ Serial.println("no encendido"); }
   
    if((millis()-tiempo)>500){    
                                    
      if(digitalRead(36)==LOW) {
        
          int contador=0;
          while(digitalRead(36)==LOW){         
            delay(10);
            yield2();     
            contador++; 
            if(contador>100){ 
                color++;
                if(color>5) { color=0; }
                encendido=false;
                break;
            }
          }      

          if(contador<100){
              encendido=!encendido;  
          }
          
          if(encendido) { reproduce(0); }
          else { reproduce(1); }
          
          tiempo=millis();
          
          
      }
    }

   
    if(encendido){
      
        leempu6050();

        boolean entra=true;

        if(dgx>umbralchoque){ spositivo=true; eje=0;detectachoque();}  
        else if(dgx<-umbralchoque){ spositivo=false; eje=0;detectachoque();}  
        else if(dgy>umbralchoque){ spositivo=true; eje=1;detectachoque();}  
        else if(dgy<-umbralchoque){ spositivo=false; eje=1;detectachoque();}  
        else if(dgz>umbralchoque){ spositivo=true; eje=2;detectachoque();}  
        else if(dgz<-umbralchoque){ spositivo=false; eje=2;detectachoque();}  
               
        if(choque){  reproduce(5); choque=false; }

        if((millis()-tiemposonido)>500){
          
          if((abs(dgx)>3000)||(abs(dgy)>3000)||(abs(dgz)>3000)){ reproduce(4); }
          else if((abs(dgx)>2000)||(abs(dgy)>2000)||(abs(dgz)>2000)){ reproduce(3); }  
          else if((abs(dgx)>1000)||(abs(dgy)>1000)||(abs(dgz)>1000)){ reproduce(2); }

        }
        
                
     //   Serial.print(ax);Serial.print(",");
     //   Serial.print(ay);Serial.print(",");
     //   Serial.print(az);Serial.print(",");        
        Serial.print(dgx);Serial.print(",");
        Serial.print(dgy);Serial.print(",");
        Serial.println(dgz);
    }
    
    delay(10);

    yield2();        
    
  
}
