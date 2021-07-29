#include "qi.h"
#include <sys/time.h>
#include <sys/times.h>
# define HIGH 1
# define LOW 0

#define PIN_NUM_QI 12
#define GPIO_OUTPUT_PIN_SEL  ((1ULL<<PIN_NUM_QI))


extern int usleep (__useconds_t __useconds);
void _delay_us(int us){
  usleep(us);
}
void _delay_ms(int ms){
  vTaskDelay(ms / portTICK_RATE_MS);
}
void digitalWrite(uint8_t pin, uint8_t state) {
  if (state)gpio_set_level(PIN_NUM_QI, 1);
  else gpio_set_level(PIN_NUM_QI, 0);

}

// void adc_init(void) {
//   ADMUX = 1 << REFS0;
//   ADMUX |= 3;
//   ADCSRA = (1 << ADEN) | (1 << ADPS2);
// }

// uint16_t adc_read(void) {
//   ADCSRA |= 1 << ADSC;
//   while (ADCSRA & (1 << ADSC));
//   return ADC;
// }

volatile uint8_t bit_state = 0;
void tx_byte(uint8_t data) {
  bit_state ^= 1;
  digitalWrite(2, bit_state);
  _delay_us(250);
  digitalWrite(2, bit_state);
  _delay_us(250);

  uint8_t parity = 0;
  for (int i = 0; i < 8; i++) {
    bit_state ^= 1;
    digitalWrite(2, bit_state);
    _delay_us(250);
    if (data & (1 << i)) {
      parity++;
      bit_state ^= 1;
    }
    digitalWrite(2, bit_state);
    _delay_us(250);
  }

  if (parity & 1) {
    parity = 0;
  } else
    parity = 1;

  bit_state ^= 1;
  digitalWrite(2, bit_state);
  _delay_us(250);

  if (parity) {
    bit_state ^= 1;
  }
  digitalWrite(2, bit_state);
  _delay_us(250);

  bit_state ^= 1;
  digitalWrite(2, bit_state);
  _delay_us(250);
  bit_state ^= 1;

  digitalWrite(2, bit_state);
  _delay_us(250);

}

void tx(uint8_t * data, int len) {
  uint8_t checksum = 0;
//  static uint8_t state = 0;
  for (int i = 0; i < 15; i++) {
    digitalWrite(2, HIGH);
    _delay_us(250);
    digitalWrite(2, LOW);
    _delay_us(250);
  }
  bit_state = 0;
  for (int i = 0; i < len; i++) {
    tx_byte(data[i]);
    checksum ^= data[i];
  }
  tx_byte(checksum);
}

static uint16_t adcvb[2];
void qi() {

  volatile uint8_t state = 0;
  gpio_config_t io_conf;
    //disable interrupt
    io_conf.intr_type = GPIO_PIN_INTR_DISABLE;
    //set as output mode
    io_conf.mode = GPIO_MODE_OUTPUT;
    //bit mask of the pins that you want to set,e.g.GPIO18/19
    io_conf.pin_bit_mask = GPIO_OUTPUT_PIN_SEL;
    //disable pull-down mode
    io_conf.pull_down_en = 0;
    //disable pull-up mode
    io_conf.pull_up_en = 1;
    //configure GPIO with the given settings
    gpio_config(&io_conf);

    gpio_set_level(PIN_NUM_QI, 1);
  while (1) {

    //uint16_t adcv = adc_read();
    uint16_t adcv = 2;
    state = 1;
    // if (adcv < 423) {
    //   state = 0;
    // }

    switch (state) {
    case 0:
      {
        // uint16_t adcv = adc_read();
        if (adcv > 423) {
          state = 1;
          _delay_ms(10);
        }
        break;
      }
    case 1:
      {
        uint8_t dt[] = {
          0x1,
          255
        };
        for (int i = 0; i < 20; i++) {
          tx(dt, 2);	//send ping response so that the transmitter identifies receiver.
          _delay_ms(10);
        }
        state = 2;
        break;
      }

    case 2:
      {
        //if(adcv > ((423*3/2))) {
        // int8_t error = 0;
        // uint16_t adcv = adc_read();
        // int16_t temp_error = 0;
        // adcvb[0] = adcvb[1];
        // adcvb[1] = adcv;
        // //if(abs(adcvb[0] - adcvb[1]) > 20)
        // // temp_error = (int16_t)((423* 3) - adcv);
        // //else 
        // temp_error = (int16_t)((423 * 2) - adcv);	//1.1v adc reference. 423 equals to 5V. (4.7/47K voltage divider)

        // temp_error /= 5;
        // if (temp_error > 127) temp_error = 127;
        // if (temp_error < -128) temp_error = -128;
        // error = (int8_t) temp_error;
        uint8_t dt[] = {
          0x3,
          (int8_t) 200
        };
        tx(dt, 2);	//send error correction packet. 0x03 is error correction packet header. 1 BYTE payload, check WPC documents for more details.
        /*} else {
         uint8_t dt[] = {0x3,(int8_t)1};
         tx(dt,2);
        }*/
      } {
        uint8_t dt[] = {0x4, 0XFF};
        tx(dt, 2);	//received power indication packet. I am not sure if this is needed or not. Please read the WPC document
					//for more details. I jut implemented and it worked. But I am not sure if this is the proper way to do it.
      }
      //    _delay_ms(10);
      break;

    }
  }
}