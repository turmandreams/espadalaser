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


//GPIO's
//#define LRC  27
//#define BCK  26
//#define DOU  25


// Configuración del bus I2S PCM5102
i2s_config_t i2sConfig;

// Buffer de salida de audio
int16_t audioBuffer[512];
size_t bytesWritten;


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
  
  gx=Wire.read()<<8|Wire.read(); //Cada valor ocupa 2 registros
  gy=Wire.read()<<8|Wire.read();
  gz=Wire.read()<<8|Wire.read();


}

void silencio(){


  int j=0;  
  
  for(;;){

    audioBuffer[j]=0;
    audioBuffer[j+1]=0;       
        
    j+=2;
    
    if(j>=sizeof(audioBuffer)/sizeof(int16_t)){                 
      
      //Serial.println(audioBuffer[j-2]);
      j=0;            
      i2s_write(I2S_NUM_0, audioBuffer, sizeof(audioBuffer), &bytesWritten, portMAX_DELAY);

      delay(1);
      yield();        
      break;
    }       
  }

   
}

void reproduce(int sonido){
 
  int j=0;  
  int i=0;
  
  for(;;){

    int16_t dato;

    if(sonido==0){  dato = espada1[i+1]<<8 | espada1[i]; }
    else if(sonido==1){  dato = espada2[i+1]<<8 | espada2[i]; }
    else if(sonido==2){  dato = espada3[i+1]<<8 | espada3[i]; }
    
    audioBuffer[j]=dato;
    audioBuffer[j+1]=dato;       
        
    j+=2;

    i+=2;
    
    
    if(j>=sizeof(audioBuffer)/sizeof(int16_t)){                 
      
      //Serial.println(audioBuffer[j-2]);
      j=0;            
      i2s_write(I2S_NUM_0, audioBuffer, sizeof(audioBuffer), &bytesWritten, portMAX_DELAY);

      delay(1);
      yield();        
    }   
    
    if((i>=espada1tam)&&(sonido==0)) { break; }
    else if((i>=espada2tam)&&(sonido==1)) { break; }
    else if((i>=espada3tam)&&(sonido==2)) { break; }
    
  }

  silencio();
  
 // Serial.println("Sale de reproducir");
  
}


void setup() {

  
  Serial.begin(115200);
  pixels.begin();
   
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
  i2sConfig.sample_rate = 44000;
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

   xTaskCreatePinnedToCore(
                    Task1code,   /* Task function. */
                    "Task1",     /* name of task. */
                    10000,       /* Stack size of task */
                    NULL,        /* parameter of the task */
                    1,           /* priority of the task */
                    &Task1,      /* Task handle to keep track of created task */
                    0);          /* pin task to core 0 */ 


  
  reproduce(0);
     
}


void Task1code( void * pvParameters ){    // en este Core recogemos las peticiones web

    
  for(;;){   
    
    for(int i=0;i<95;i++){    
      pixels.setPixelColor(i,pixels.Color(0,0,150));   
      delay(5);    
      pixels.show();
    }    

    delay(5000);
  
    for(int i=95;i>=0;i--){
      pixels.setPixelColor(i,pixels.Color(0,0,0));      
      delay(5);
      //Serial.print(".");
      pixels.show(); 
    }

    delay(1000);

  
  
  } 
}

void loop() {    
    
    

    leempu6050();

    if((gx>20000)||(gy>20000)||(gz>20000)){ reproduce(1); }
    if((gx<-20000)||(gy<-20000)||(gz<-20000)){ reproduce(2); }
    
    Serial.print(ax);Serial.print(",");
    Serial.print(ay);Serial.print(",");
    Serial.print(az);Serial.print(",");
    Serial.print(gx);Serial.print(",");
    Serial.print(gy);Serial.print(",");
    Serial.println(gz);
  
    delay(10);
    
    esp_task_wdt_reset();         
    
  
}
