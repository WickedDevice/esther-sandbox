#define WINDOW_DURATION_US (10UL*1000UL*1000UL)
#define SAMPLE_RATE_US     (1000UL)
#define NUM_SAMPLES        (WINDOW_DURATION_US / SAMPLE_RATE_US)
#define SAMPLE_BUFFER_SIZE (NUM_SAMPLES / 16UL) // 16 samples per buffer entry
#define TIMER2_PRELOAD     (131) // for prescaler = 64 to get 500us rate

// this is a bit-packed buffer of samples
volatile uint16_t sample_buffer1[SAMPLE_BUFFER_SIZE] = {0};
volatile uint16_t sample_buffer2[SAMPLE_BUFFER_SIZE] = {0};
volatile uint32_t sample_index = 0;




//long _previousMillis = 0;
//long _interval = 1000;

/*
void setup(void){
  wf.begin();
  Serial.begin(115200);
  Serial.print("Hello World\n#Samples: ");
  Serial.println(NUM_SAMPLES);
  Serial.print("Buffer Size: ");
  Serial.println(SAMPLE_BUFFER_SIZE);
  
  setupTimer2Interrupt();
  pinMode(25, INPUT); //PORTD5
  pinMode(26, INPUT); //PORTD6
  
  Serial.println("#0a, #1a, #0b, #1b");
}
*/

/*
void loop(void){
  unsigned long currentMillis = millis();
  if(currentMillis - _previousMillis > _interval) {
    _previousMillis = currentMillis;   
    cli();
    Serial.print(num_zeros1);
    Serial.print(", ");
    Serial.print(num_ones1);
    Serial.print(", ");    
    Serial.print(num_zeros2);
    Serial.print(", ");
    Serial.print(num_ones2);     
    Serial.println();
    sei();
  }

  updateCounts();
  
  
}

*/

void setupTimer2Interrupt(void){
  TCCR2A = 0x00;
  TCCR2B = 0x04; // prescaler = 64
  TIMSK2 = 0x01; // enable overflow interrupt
  TCNT2  = TIMER2_PRELOAD;
}

ISR(TIMER2_OVF_vect){
  TCNT2  = TIMER2_PRELOAD;

  uint32_t sample_buffer_index = (sample_index / 16);
  uint16_t sample_buffer_mask  = (((uint16_t) 1) << (sample_index & 15));
  
  //make sure to check pin definitions wrt particulate sensor!
  // sample the buffer1
  if((PIND & _BV(2)) == 0){ // the signal is low
    sample_buffer1[sample_buffer_index] &= ~(sample_buffer_mask);
  }
  else{
    sample_buffer1[sample_buffer_index] |= (sample_buffer_mask);
  }

  if((PIND & _BV(3)) == 0){ // the signal is low
    sample_buffer2[sample_buffer_index] &= ~(sample_buffer_mask);
  }
  else{
    sample_buffer2[sample_buffer_index] |= (sample_buffer_mask);
  }

  sample_index++;
  if(sample_index >= NUM_SAMPLES){
    sample_index = 0;
  }      
  
}

void updateCounts(void){

  num_ones1 = 0;
  num_zeros1 = 0;   
  num_ones2 = 0;
  num_zeros2 = 0;     
  
  for(uint32_t ii = 0; ii < SAMPLE_BUFFER_SIZE; ii++){
    for(uint8_t jj = 0; jj < 16; jj++){
      if((sample_buffer1[ii] & (1 << jj)) == 0){
        num_zeros1++; 
      }
      else{
        num_ones1++; 
      }

      if((sample_buffer2[ii] & (1 << jj)) == 0){
        num_zeros2++; 
      }
      else{
        num_ones2++; 
      }      
    }
  }  
}

