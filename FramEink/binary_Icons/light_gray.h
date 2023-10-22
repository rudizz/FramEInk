// Ogni elemento (1 byte) è composto da due colori.
// Ogni colore è definito da 3 bit in cui il LSB (quello più a destra) viene scartato.
const uint8_t light_gray[] PROGMEM = {
0x2e,0x2e,
0xe2,0xe2,
};
int light_gray_w = 4;
int light_gray_h = 2;