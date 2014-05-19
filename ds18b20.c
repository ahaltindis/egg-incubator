#define ONE_WIRE_PIN PIN_A0

void onewire_reset() {
   output_low(ONE_WIRE_PIN);
   delay_us( 500 ); //1-wire resetleme için sifira çekilir
   output_float(ONE_WIRE_PIN); //1-wire bir yapilir
   delay_us( 500 ); // sensörün hazirlanmasi için beklenir.
   output_float(ONE_WIRE_PIN);
} 

void onewire_write(int data) {
   int count;
   for (count=0; count<8; ++count) {
      output_low(ONE_WIRE_PIN);
      delay_us( 2 ); // Sensöre yazma islemine baslamak için  1-wire sifira çekilir.
      output_bit(ONE_WIRE_PIN, shift_right(&data,1,0)); // yazilacak bilgi 1-wire'da
      delay_us( 60 ); // Yazma islemi zamani doldurulur.
      output_float(ONE_WIRE_PIN); // 1-wire bir yapilir,
      delay_us( 2 ); // 1us'den fazla beklenir.
   }
} 

int onewire_read() {
   int count, data;
   for (count=0; count<8; ++count) {
      output_low(ONE_WIRE_PIN);
      delay_us( 2 ); //Sensördem okuma islemi içinl 1-wire sifira çekilir.
      output_float(ONE_WIRE_PIN); //1-wire bir yapilir,
      delay_us( 8 ); // Sensörün kendine gelmesi beklenir,
      shift_right(&data,1,input(ONE_WIRE_PIN)); // sonuc bilgisi alinir.
      delay_us( 120 ); //Okuma islemi zamani doldurulur.
   }
   return( data );
} 

float ds1820_read() {
   int8 busy=0, temp1, temp2;
   signed int16 temp3;
   float result;
   onewire_reset();
   onewire_write(0xCC);
   onewire_write(0x44);
   while (busy == 0)
   busy = onewire_read();
   onewire_reset();
   onewire_write(0xCC);
   onewire_write(0xBE);
   temp1 = onewire_read();
   temp2 = onewire_read();
   temp3 = make16(temp2, temp1);
   result = (float) temp3 / 16.0;
   delay_ms(200);
   return(result);
} 
