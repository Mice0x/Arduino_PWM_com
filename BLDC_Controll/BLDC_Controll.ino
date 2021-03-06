#include <Wire.h>

#define ADDRESS       0x08
#define PWM_MIN_DUTY  50


//benoetigte variablen fuer das messen wenn der Motor stehen bleibt
long start_t = 0;
int step_t = 0; // die die fuer einen Step benoetigt Zeit
long x = 0;
int j = 0;
long y = 0;
long s = 0; //standartabweichung
long median = 0;

int cnt = 0; // counter

bool motor_on = false;
int pwm_receive;
byte dir_receive;
byte bldc_step = 0, motor_speed;
unsigned int i;
void setup() {
  DDRD  |= 0x38;           // Configure pins 3, 4 and 5 as outputs
  PORTD  = 0x00;
  DDRB  |= 0x0E;           // Configure pins 9, 10 and 11 as outputs
  PORTB  = 0x31;
  // Timer1 module setting: set clock source to clkI/O / 1 (no prescaling)
  TCCR1A = 0;
  TCCR1B = 0x01;
  // Timer2 module setting: set clock source to clkI/O / 1 (no prescaling)
  TCCR2A = 0;
  TCCR2B = 0x01;
  // Analog comparator setting
  ACSR   = 0x10;           // Disable and clear (flag bit) analog comparator interrupt

  Wire.begin(ADDRESS);
  Wire.onReceive(pwmRcv);
}


// Analog comparator ISR
ISR (ANALOG_COMP_vect) {
  // BEMF debounce
  for (i = 0; i < 10; i++) {
    if (bldc_step & 1) {
      if (!(ACSR & 0x20)) i -= 1;
    }
    else {
      if ((ACSR & 0x20))  i -= 1;
    }
  }
  if (dir_receive == 1) {
    bldc_move();
  } else {
    bldc_reverse();
  }
  step_t = micros() - start_t; // ~ Zeit bis zum Naechsten step
  start_t = micros();
  bldc_step++;
  bldc_step %= 6;
}

void bldc_move() {       // BLDC motor commutation function
  switch (bldc_step) {
    case 0:
      AH_BL();
      BEMF_C_RISING();
      break;
    case 1:
      AH_CL();
      BEMF_B_FALLING();
      break;
    case 2:
      BH_CL();
      BEMF_A_RISING();
      break;
    case 3:
      BH_AL();
      BEMF_C_FALLING();
      break;
    case 4:
      CH_AL();
      BEMF_B_RISING();
      break;
    case 5:
      CH_BL();
      BEMF_A_FALLING();
      break;
  }
}

void bldc_reverse() {
  switch (bldc_step) {
    case 5:
      AH_BL();
      BEMF_C_RISING();
      break;
    case 4:
      AH_CL();
      BEMF_B_FALLING();
      break;
    case 3:
      BH_CL();
      BEMF_A_RISING();
      break;
    case 2:
      BH_AL();
      BEMF_C_FALLING();
      break;
    case 1:
      CH_AL();
      BEMF_B_RISING();
      break;
    case 0:
      CH_BL();
      BEMF_A_FALLING();
      break;
  }
}

void loop() {
  if (IsMotorStuck()) {
    rapidStop();
    delay(1000);
  }
  if (!(motor_on)) {
    motorStart();
    delay(500);
  }
  if (pwm_receive < PWM_MIN_DUTY) {
    motorStop();
  }
  while (motor_speed < pwm_receive && motor_speed < 255 && motor_on) {
    motor_speed++;
    SET_PWM_DUTY(motor_speed);
    delay(5);
  }
  while (motor_speed > pwm_receive  && motor_speed > 0 && motor_on) {
    motor_speed--;
    SET_PWM_DUTY(motor_speed);
    delay(5);
  }
}

bool IsMotorStuck() {
  bool motor_stuck = false;
  if (step_t >= 600) {
    motor_stuck = true;
  }
  return motor_stuck;
}

void motorStart() {
  if (pwm_receive >= PWM_MIN_DUTY) {
    SET_PWM_DUTY(pwm_receive);
    i = 5000;
    // Motor start
    while (i > 100) {
      delayMicroseconds(i);
      if (dir_receive == 1) {
        bldc_move();
      } else {
        bldc_reverse();
      }
      bldc_step++;
      bldc_step %= 6;
      i = i - 20;
    }
    motor_speed = pwm_receive;
    motor_on = true;
    s = 0;
    median = 0;
    ACSR |= 0x08;                    // Enable analog comparator interrupt
  }
}

void rapidStop() {
  ACSR   = 0x10;
  SET_PWM_DUTY(0);
  motor_on = false;
}

void motorStop() {
  while (motor_speed > 20) {
    motor_speed--;
    SET_PWM_DUTY(motor_speed);
    delay(20);
  }
  SET_PWM_DUTY(0);
  ACSR   = 0x10;
  motor_on = false;
  delay(100);
}

void pwmRcv(int numBytes) {
  while (Wire.available()) {
    pwm_receive = Wire.read();
    dir_receive = Wire.read();
  }
}

void BEMF_A_RISING() {
  ADCSRB = (0 << ACME);    // Select AIN1 as comparator negative input
  ACSR |= 0x03;            // Set interrupt on rising edge
}
void BEMF_A_FALLING() {
  ADCSRB = (0 << ACME);    // Select AIN1 as comparator negative input
  ACSR &= ~0x01;           // Set interrupt on falling edge
}
void BEMF_B_RISING() {
  ADCSRA = (0 << ADEN);   // Disable the ADC module
  ADCSRB = (1 << ACME);
  ADMUX = 2;              // Select analog channel 2 as comparator negative input
  ACSR |= 0x03;
}
void BEMF_B_FALLING() {
  ADCSRA = (0 << ADEN);   // Disable the ADC module
  ADCSRB = (1 << ACME);
  ADMUX = 2;              // Select analog channel 2 as comparator negative input
  ACSR &= ~0x01;
}
void BEMF_C_RISING() {
  ADCSRA = (0 << ADEN);   // Disable the ADC module
  ADCSRB = (1 << ACME);
  ADMUX = 3;              // Select analog channel 3 as comparator negative input
  ACSR |= 0x03;
}
void BEMF_C_FALLING() {
  ADCSRA = (0 << ADEN);   // Disable the ADC module
  ADCSRB = (1 << ACME);
  ADMUX = 3;              // Select analog channel 3 as comparator negative input
  ACSR &= ~0x01;
}

void AH_BL() {
  PORTD &= ~0x28;
  PORTD |=  0x10;
  TCCR1A =  0;            // Turn pin 11 (OC2A) PWM ON (pin 9 & pin 10 OFF)
  TCCR2A =  0x81;         //
}
void AH_CL() {
  PORTD &= ~0x30;
  PORTD |=  0x08;
  TCCR1A =  0;            // Turn pin 11 (OC2A) PWM ON (pin 9 & pin 10 OFF)
  TCCR2A =  0x81;         //
}
void BH_CL() {
  PORTD &= ~0x30;
  PORTD |=  0x08;
  TCCR2A =  0;            // Turn pin 10 (OC1B) PWM ON (pin 9 & pin 11 OFF)
  TCCR1A =  0x21;         //
}
void BH_AL() {
  PORTD &= ~0x18;
  PORTD |=  0x20;
  TCCR2A =  0;            // Turn pin 10 (OC1B) PWM ON (pin 9 & pin 11 OFF)
  TCCR1A =  0x21;         //
}
void CH_AL() {
  PORTD &= ~0x18;
  PORTD |=  0x20;
  TCCR2A =  0;            // Turn pin 9 (OC1A) PWM ON (pin 10 & pin 11 OFF)
  TCCR1A =  0x81;         //
}
void CH_BL() {
  PORTD &= ~0x28;
  PORTD |=  0x10;
  TCCR2A =  0;            // Turn pin 9 (OC1A) PWM ON (pin 10 & pin 11 OFF)
  TCCR1A =  0x81;         //
}

void SET_PWM_DUTY(byte duty) {
  OCR1A  = duty;                   // Set pin 9  PWM duty cycle
  OCR1B  = duty;                   // Set pin 10 PWM duty cycle
  OCR2A  = duty;                   // Set pin 11 PWM duty cycle
}
