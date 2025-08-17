/*
 * main.cpp
 * 
 *
 * Chip used: ATTiny13A
 * The internal oscillator and no prescaling is used for this project.
 * The state of the chip's fuses should be (E:FF, H:FF, L:6A) when not locked.
 *
 *								 _________
 * PIN1 - N/C       		   _|	 O    |_		PIN8 - VCC
 * PIN2	- Button     		   _|		  |_		PIN7 - N/C
 * PIN3	- Serial to DFPlayer   _|ATTiny13A|_		PIN6 - Power mosfet
 * PIN4	- Ground			   _|		  |_		PIN5 - Virtual GND
 *							    |_________|
 */ 

#define F_CPU 9600000UL
#include <avr/io.h>
#include <util/delay.h>
#include <avr/sleep.h>
#include <avr/interrupt.h>

// -------- User settings --------
#define FOLDER1        0x01
#define FILE1_MIN      1
#define FILE1_MAX      27
#define FOLDER2        0x02
#define FILE2_MIN      1
#define FILE2_MAX      43
#define WEIGHT_FOLDER1 90          // % chance for folder 1
#define SLEEP_TIMEOUT  60000       // 60 seconds

// -------- Pins --------
// PB4: TX ? DFPlayer RX (via ~1k)
// PB3: Button to GND (pull-up enabled)
// PB1: DFPlayer power control (HIGH=ON, LOW=OFF)  << as requested
#define TX_PORT PORTB
#define TX_DDR  DDRB
#define TX_BIT  PORTB4
#define TX_DDB  DDB4

#define PWR_PORT PORTB
#define PWR_DDR  DDRB
#define PWR_BIT  PORTB1
#define PWR_DDB  DDB1

#define BTN_PIN  PINB
#define BTN_PORT PORTB
#define BTN_DDR  DDRB
#define BTN_BIT  PINB3
#define BTN_DDB  DDB3

// -------- Power control helpers --------
static inline void df_power_on(void)  { PWR_PORT |=  (1 << PWR_BIT); }
static inline void df_power_off(void) { PWR_PORT &= ~(1 << PWR_BIT); }

// -------- RNG (16-bit LFSR) with Timer0 seeding on first use --------
static uint16_t lfsr = 0xACE1;
static uint8_t  seeded = 0;

uint16_t rand16(void) {
	uint16_t lsb = lfsr & 1;
	lfsr >>= 1;
	if (lsb) lfsr ^= 0xB400;  // taps 16,14,13,11
	return lfsr;
}
static inline void seed_rng_once(void) {
	if (!seeded) { lfsr ^= (uint16_t)TCNT0; seeded = 1; }
}

// -------- Bit-banged UART TX @ 9600-8N1 on PB4 --------
static inline void tx_byte(uint8_t b) {
	// start bit
	TX_PORT &= ~(1 << TX_BIT);
	_delay_us(104);
	// data bits LSB first
	for (uint8_t i = 0; i < 8; i++) {
		if (b & 1) TX_PORT |=  (1 << TX_BIT);
		else       TX_PORT &= ~(1 << TX_BIT);
		_delay_us(104);
		b >>= 1;
	}
	// stop bit
	TX_PORT |= (1 << TX_BIT);
	_delay_us(104);
}

static void send_pkt(uint8_t cmd, uint8_t dh, uint8_t dl) {
	uint16_t sum = 0 - (0xFF + 0x06 + cmd + 0x00 + dh + dl);
	uint8_t p[10] = {0x7E,0xFF,0x06,cmd,0x00,dh,dl,(uint8_t)(sum>>8),(uint8_t)sum,0xEF};
	for (uint8_t i = 0; i < 10; i++) tx_byte(p[i]);
}

// -------- Pin-change ISR to wake from sleep --------
volatile uint8_t wake_flag = 0;
ISR(PCINT0_vect) { wake_flag = 1; }

// -------- DFPlayer init sequence (after power-up) --------
static void df_init(void) {
	_delay_ms(2000);                 // DFPlayer boot/index SD
	send_pkt(0x09, 0x00, 0x02);      // Select TF
	_delay_ms(300);
	send_pkt(0x06, 0x00, 0x1C);      // Volume = 20/30
	_delay_ms(50);
}

// -------- Random play with 90/10 weighting --------
static void play_random(void) {
	seed_rng_once();
	uint8_t pick = rand16() % 100;
	uint8_t folder, file;
	if (pick < WEIGHT_FOLDER1) {
		folder = FOLDER1;
		file = FILE1_MIN + (rand16() % (FILE1_MAX - FILE1_MIN + 1));
		} else {
		folder = FOLDER2;
		file = FILE2_MIN + (rand16() % (FILE2_MAX - FILE2_MIN + 1));
	}
	send_pkt(0x0F, folder, file);
}

int main(void) {
	// --- GPIO setup ---
	TX_DDR  |= (1 << TX_DDB);    // TX out
	PWR_DDR |= (1 << PWR_DDB);   // DF power control out
	BTN_DDR &= ~(1 << BTN_DDB);  // Button in
	BTN_PORT |= (1 << BTN_DDB);  // Pull-up on button
	TX_PORT  |= (1 << TX_BIT);   // UART idle high

	// Power DFPlayer ON at boot
	df_power_on();

	// Pin change interrupt on PB3 (button) to wake from sleep
	GIMSK |= (1 << PCIE);
	PCMSK |= (1 << BTN_BIT);
	sei();

	// Timer0 free-run @ F_CPU/64 for entropy
	TCCR0A = 0;
	TCCR0B = (1 << CS01) | (1 << CS00); // clk/64
	TCNT0 = 0;

	while (1) {
		// Ensure DFPlayer is powered and initialized
		df_init();

		// If we just woke by button, play immediately
		if (wake_flag) {
			wake_flag = 0;
			play_random();
		}

		// ---- Main active loop ----
		uint16_t idle_ms = 0;
		while (1) {
			// Button press ? play random, reset idle timer
			if (!(BTN_PIN & (1 << BTN_BIT))) {
				_delay_ms(30);
				if (!(BTN_PIN & (1 << BTN_BIT))) {
					play_random();
					while (!(BTN_PIN & (1 << BTN_BIT))); // wait release
					_delay_ms(50);
					idle_ms = 0;
				}
			}

			_delay_ms(10);
			idle_ms += 10;

			if (idle_ms >= SLEEP_TIMEOUT) {
				// --- Go to low power ---
				// 1) Hard power off DFPlayer
				df_power_off();
				_delay_ms(10);  // let rails collapse

				// 2) Put ATtiny into deep sleep (wake on button PCINT)
				set_sleep_mode(SLEEP_MODE_PWR_DOWN);
				sleep_enable();
				sleep_cpu();    // Zzzz
				sleep_disable();

				// 3) Woke by button ? restore DFPlayer power and play
				df_power_on();
				df_init();      // wait & re-init TF/volume
				wake_flag = 0;  // consume wake
				play_random();
				break;          // back to active loop with fresh idle timer
			}
		}
	}
}
