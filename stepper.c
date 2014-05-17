#byte port_b = 6

#define FOUR_PHASE TRUE

#ifdef FOUR_PHASE

byte const POSITIONS[4] = {0b0101,
                           0b1001,
                           0b1010,
                           0b0110};
#else
byte const POSITIONS[8] = {0b0101,
                           0b0001,
                           0b1001,
                           0b1000,
                           0b1010,
                           0b0010,
                           0b0110,
                           0b0100};
#endif

void drive_stepper(BYTE speed, char dir, BYTE steps) {
   static BYTE stepper_state = 0;
   BYTE i;

   for(i=0; i<steps; ++i) {
      delay_ms(speed);
      set_tris_b(0x0f);
      port_b = POSITIONS[ stepper_state ] << 4;
      if(dir!='B')
         stepper_state=(stepper_state+1)&(sizeof(POSITIONS)-1);
      else
         stepper_state=(stepper_state-1)&(sizeof(POSITIONS)-1);
   }
}
