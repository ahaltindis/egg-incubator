//##### EGG INCUBATOR #### //
// PIC16f877A              //
// CCS C                   //
// Maintainer: ahmeta      //
//########################//

#include <16f877a.h>
#device ADC=10

#fuses HS,NOWDT,NOPROTECT,NOBROWNOUT,NOLVP
#use delay(clock = 4000000)
#use fast_io(a)

#ifndef     rtc_sclk
// DS1302 entegresi icin kullanilacak pin numaralari
   #define     rtc_sclk   pin_c1 // sclk (7)
   #define     rtc_io     pin_c2 // IO (6)     
   #define     rtc_rst    pin_c0 // ce (5)
#endif     

#include <lcd.c>
#include <ds1302.c>
#include <stepper.c>

#define BUTTON_MENU   PIN_A1
#define BUTTON_UP     PIN_A2
#define BUTTON_DOWN   PIN_A3
#define BUTTON_CHG    PIN_A4

#define LAMP_PIN      PIN_C4
#define FAN_PIN       PIN_C5

#define TOTAL_SETUP_SCREEN 6  // Ayarlar kisminda gezilecek ayar sekmesi sayisi
#define TOTAL_INFO_SCREEN 5   // Sirayla gosterilecek bilgi ekran sayisi
#define MAIN_SCREEN_WAIT 77 // Her ekranin gosterilme suresi Hesap: 77*65ms = 5sn

#define MOTOR_STEPS_INTERVAL 15 // Her adim arasinda beklenilecek sure -> motor hizini belirler.
#define MOTOR_MAX_STEPS 200   // Motorun tam bir tur icin gerekli adim sayisi
#define MOTOR_TEST_STEP 5     // Test modunda motorun her seferinde atacagi adim.

int8 cur_screen_no = 0; // Aktif olan ekran no, 0-> Ana Ekran  1,2,3,... -> Ayarlar Ekrani
int8 cur_info_no = 1; // Ana ekranda aktif olan bilgi ekrani no

int8 prev_screen = 0;    // Ekran degismedikce ekran yenilenmesi yapmamasi icin onceki menu degeri
int1 lcd_update = 0;     // Lcd yenilenmesi gerekiyor mu? Not: Ayarlardan sonra
int1 lcd_froze = 0;      // Lcd ekranda o anki bilgi ekrani dondurulsun mu?

int8 settings_no = 1; // Her bir ayar sekme ekraninda, degistirilebilir bolge sayisi

unsigned int8 time_hour, time_min, time_sec;    // Saat bilgileri degiskeni
unsigned int8 date_day, date_mth, date_year, date_dow;   // Tarih bilgileri degiskeni

float temp;    // Sicaklik degerini tutan degisken

int1 heater = 0;   // Isitma durum degiskeni
int1 cooler = 0;   // Sogutma durum degiskeni

int8 lint_count = 0; // Lcd Kesmesinde olusan kesmeleri sayan degisken

unsigned int8 motor_step_count = 50; // Her periyot vaktinde motorun dondurulecegi adim sayisi

// ###### EEPROM da saklanan degiskenler ########

int8 motor_period;  // Motorun dondurulme periyodu

int8 motor_cur_step; // Step motorun o anki adimini tutan degisken

int8 next_turn_hr;  // Bir dahaki motorun dondurulecegi saat
int8 next_turn_min; // Bir dahaki motorun dondurulecegi dakika

int8 temp_ideal;    // Kutunun icerisinin ideal sicakligi
int8 temp_sensivity;   // Sicakliga karsi duyarlilik


// #################################


void calculate_next_turn(){
   next_turn_hr = (time_hour + motor_period) % 24;
   next_turn_min = time_min;
  
  // Eger periyot 24 ise ayni dakikada tekrar dondurme yapmadigindan
  // emin olmak icin, dakika 1 eksiltilir. 
   if ( motor_period == 24 ) {  
      if ( next_turn_min == 0 ){
         next_turn_min = 59;         
         if ( next_turn_hr == 0 )
            next_turn_hr = 23;
         else
            next_turn_hr--;
      }
      else
         next_turn_min--;
   }
   write_eeprom(2, next_turn_hr);
   write_eeprom(3, next_turn_min);   
}

void motor_move_relative(int8 step, char dir) {
   if ( dir == 'F' ) {
      for( int i = 0; i < step; i++ ) {
         motor_cur_step++;
         if ( motor_cur_step >= 200 )
            motor_cur_step = 0;            
         drive_stepper(MOTOR_STEPS_INTERVAL, 'F', 1);
      }
   }
   else
   {
      for( int i = 0; i < step; i++ ) {         
         if ( motor_cur_step == 0 )
            motor_cur_step = 199;      
         else
            motor_cur_step--;
         drive_stepper(MOTOR_STEPS_INTERVAL, 'B', 1);
      }   
   }
   write_eeprom(1, motor_cur_step);
}

void set_current_time() {
   rtc_set_datetime(date_day, date_mth, date_year, date_dow, time_hour, time_min);
}

float ds_read_temp() {
// Sicaklik okuma fonksiyonu   
   unsigned long int ds_raw;
   float ds_temp;
   
   ds_raw = read_adc();
   ds_temp = ds_raw * 0.4883;
   
   return ds_temp;
}

#INT_EXT
void menu_int() {
// Basilan butonlari kontrol eden harici RB0 kesmesi

// ############  BUTON MENU UYGULAMALARI ########### //
   if(input(BUTTON_MENU)) {
      cur_screen_no++;     // Her menu butonuna basildiginda ekran no 1 arttir
      
      if (cur_screen_no > TOTAL_SETUP_SCREEN) {
      // Eger belirlenen toplam ekran sayisi asilirsa, ana ekrana git ve yanip sonen imleci gizle
         cur_screen_no = 0;         
         lcd_send_byte(0, 0x0c);  
      }
      else {   
      // Ana ekrandan ayarlar ekranina gecilirse, yanip sonen imleci ac
         lcd_send_byte(0, 0x0d);
      }
   }
// ######### //
 
// ############  BUTON YUKARI UYGULAMALARI ########### //
   if(input(BUTTON_UP) && cur_screen_no > 0) {
   
      // SAAT AYARLAMA MENU && BUTON YUKARI
      if ( cur_screen_no == 1 ) {
         if ( settings_no == 1) {
            // Saati Arttir
            time_hour++;
            if ( time_hour > 23 )
               time_hour = 0;                        
         }         
         else if ( settings_no == 2) {
            // Dakikayi Arttir
            time_min++;
            if ( time_min > 59 )
               time_min = 0;                    
         }
         set_current_time();
         calculate_next_turn();
      }
      
      // TARIH AYARLAMA MENU && BUTON YUKARI
      else if ( cur_screen_no == 2 ) {
         if ( settings_no == 1) {
            // Gun u arttir
            date_day++;
            if ( date_day > 31 )
               date_day = 1;                        
         }         
         else if ( settings_no == 2) {
            // Ay i Arttir
            date_mth++;
            if ( date_mth > 12 )
               date_mth = 1;                    
         }
         else if ( settings_no == 3) {
            // Yil i Arttir
            date_year++;
            if ( date_year > 99 )
               date_year = 0;                    
         }
         set_current_time();
      }
      
      // PERIYOT AYARLAMA MENU && BUTON YUKARI
      else if ( cur_screen_no == 3 ) {   
         motor_period++;
         if ( motor_period > 24 )
            motor_period = 1;  
         write_eeprom(0, motor_period);
      }
      
      // TERMOSTAT AYARLARMA MENU && BUTON YUKARI
      else if ( cur_screen_no == 4) {
         if ( settings_no == 1) {
            // Ideal Arttir      
            temp_ideal++;
            if ( temp_ideal > 60 ) 
               temp_ideal = 10;
            write_eeprom(4, temp_ideal);
         }         
         else if ( settings_no == 2) {
            // Hassasiyet Arttir
            temp_sensivity++;
            if ( temp_sensivity > 9 )
               temp_sensivity = 1;
            write_eeprom(5, temp_sensivity);
         }
      }
      
      // HARICI TEST MENU && BUTON YUKARI
      else if ( cur_screen_no == 5 ) {   
         if ( settings_no == 1) {
            heater = !heater;            
         }         
         else if ( settings_no == 2) {
            cooler = !cooler;         
         }
      }
      
      // MOTOR TEST MENU && BUTON YUKARI
      else if ( cur_screen_no == 6 )  
         motor_move_relative(MOTOR_TEST_STEP 'F');
      lcd_update = 1;
   }
   else if(input(BUTTON_UP) && cur_screen_no == 0)
   {
      cur_info_no++;      
      if ( cur_info_no > TOTAL_INFO_SCREEN )
         cur_info_no = 1;   
   }
// ######### //

// ############  BUTON ASAGI UYGULAMALARI ########### //
   if(input(BUTTON_DOWN) && cur_screen_no > 0) {
   
      // SAAT AYARLAMA MENU && BUTON ASAGI
      if ( cur_screen_no == 1 ) {
         if ( settings_no == 1) {
            // Saati Azalt
            if ( time_hour == 0 )
               time_hour = 23;
            else
               time_hour--;
         }         
         else if ( settings_no == 2) {
            // Dakikayi Azalt
            if ( time_min == 0 )
               time_min = 59;
            else
               time_min--;                                
         }
         set_current_time();
         calculate_next_turn();
      }
      
      // TARIH AYARLAMA MENU && BUTON ASAGI
      else if ( cur_screen_no == 2 ) {
         if ( settings_no == 1) {
            // Gun u azalt
            date_day--;
            if ( date_day < 1 )
               date_day = 31;               
         }         
         else if ( settings_no == 2) {
            // Ay i Azalt
            date_mth--;
            if ( date_mth < 1 )
               date_mth = 12;                    
         }
         else if ( settings_no == 3) {
            // Yil i Azalt            
            if ( date_year == 0 )
               date_year = 99;
            else
               date_year--;
         }
         set_current_time();
      }
      
      // PERIYOT AYARLAMA MENU && BUTON ASAGI
      if ( cur_screen_no == 3 ) {   
         motor_period--;
         if ( motor_period ==  0 )
            motor_period = 24;    
         write_eeprom(0, motor_period);
      }
      
      // TERMOSTAT AYARLARMA MENU && BUTON ASAGI
      else if ( cur_screen_no == 4) {
         if ( settings_no == 1) {
            // Ideal Azalt      
            temp_ideal--;
            if ( temp_ideal < 10 ) 
               temp_ideal = 60;
            write_eeprom(4, temp_ideal);
         }         
         else if ( settings_no == 2) {
            // Hassasiyet Azalt
            temp_sensivity--;
            if ( temp_sensivity < 1 )
               temp_sensivity = 9;
            write_eeprom(5, temp_sensivity);
         }      
      }
      
      // HARICI TEST MENU && BUTON ASAGI
      else if ( cur_screen_no == 5 ) {   
         if ( settings_no == 1) {
            heater = !heater;            
         }
         
         else if ( settings_no == 2) {
            cooler = !cooler;         
         }
      }
      
      // MOTOR TEST MENU && BUTON ASAGI
      else if ( cur_screen_no == 6 )   
         motor_move_relative(MOTOR_TEST_STEP 'B');
      lcd_update = 1;
   }   
   else if(input(BUTTON_DOWN) && cur_screen_no == 0)
   {      
      cur_info_no--;      
      if ( cur_info_no < 1 )
         cur_info_no = TOTAL_INFO_SCREEN;   
   }
// ######### //

// ############  BUTON CHG UYGULAMALARI ########### //      
   if(input(BUTTON_CHG) && cur_screen_no > 0) {
   
      // SAAT AYARLAMA MENU && CHG BUTTON
      if ( cur_screen_no == 1 ) {
         settings_no++;
         
         if ( settings_no > 2 )
            settings_no = 1;
            
         switch(settings_no)
         {
            case 1:
               lcd_gotoxy(8,2);            
               break;
               
            case 2:
               lcd_gotoxy(11,2);            
               break;
         }
      }
      
      // TARIH AYARLAMA MENU && CHG BUTTON
      else if ( cur_screen_no == 2 ) {
         settings_no++;
         
         if ( settings_no > 3 )
            settings_no = 1;
            
         switch(settings_no)
         {
            case 1:
               lcd_gotoxy(9,2);            
               break;               
            case 2:
               lcd_gotoxy(12,2);            
               break;
            case 3:
               lcd_gotoxy(15,2);            
               break;
         }
      }
      
      // PERIYOT AYARLAMA MENU && CHG BUTTON
      else if ( cur_screen_no == 3 ) {   
         settings_no = 1;
      }
      
      // TERMOSTAT AYARLARMA MENU && CHG BUTTON
      else if ( cur_screen_no == 4) {
         settings_no++;
         
         if ( settings_no > 2 )
            settings_no = 1;
            
         switch(settings_no)
         {
            case 1:
               lcd_gotoxy(9,2);            
               break;
               
            case 2:
               lcd_gotoxy(16,2);            
               break;
         }      
      }
      
      // HARICI TEST MENU && CHG BUTTON
      else if ( cur_screen_no == 5 ) {   
          settings_no++;
         
         if ( settings_no > 2 )
            settings_no = 1;
            
         switch(settings_no)
         {
            case 1:
               lcd_gotoxy(7,2);            
               break;
               
            case 2:
               lcd_gotoxy(16,2);            
               break;
         }
      }
      
      // MOTOR TEST MENU && CHG BUTTON
      else if ( cur_screen_no == 6 ) { 
         settings_no = 1;
         motor_cur_step = 0;
         lcd_update = 1;
         write_eeprom(1, motor_cur_step);
      }
   }
   else if(input(BUTTON_CHG) && cur_screen_no == 0)
   {
      lcd_froze = !lcd_froze;
   }
// ######### //
}

#INT_TIMER0
void lcd_int() { // Kesme Periyodu = 65ms
// LCD Ekrandaki yazilari gosteren kesme fonksiyonu
   set_timer0(0);   
   lint_count++;
   
   if (lint_count == MAIN_SCREEN_WAIT ) {
      lint_count = 0;
      if ( lcd_froze == 0 )
         cur_info_no++;      
      if ( cur_info_no > TOTAL_INFO_SCREEN )
         cur_info_no = 1;
   }
   
   if ( cur_screen_no == 0 ) {
      lcd_gotoxy(1,1);
      if (lcd_froze){        
         printf(lcd_putc, " # DONDURULDU #      ");      
      }
      else {         
         printf(lcd_putc, "KULUCKA MAKINESI      ");      
      }     
// ################ BILGI EKRANI KISMI ################      
      switch(cur_info_no)
      {
         case 1:
            lcd_gotoxy(1,2);            
            printf(lcd_putc, "Saat: %02d:%02d:%02d   ", time_hour, time_min, time_sec);
            break;
         case 2:            
            lcd_gotoxy(1,2);            
            printf(lcd_putc, "Tarih: %02d/%02d/%02d   ", date_day, date_mth, date_year);
            break;
         case 3:            
            lcd_gotoxy(1,2);
            printf(lcd_putc, "Sicaklik: %2.1f C       ", temp);
            break;
         case 4:
            lcd_gotoxy(1,2);
            printf(lcd_putc, "ISITMA: %1u FAN: %1u     ", heater, cooler);
            break;
         case 5:
            lcd_gotoxy(1,2);
            printf(lcd_putc, "D. Vakti: %02d:%02d       ", next_turn_hr, next_turn_min);
            break;
         default:
            cur_info_no = 1;
            break;
      }
   }

// ########## AYARLAR KISMI ##################
   else {
      if(cur_screen_no != prev_screen || lcd_update) {      
         switch(cur_screen_no)
         {
            case 1:       
               lcd_putc("\f");
               lcd_gotoxy(1,1);
               printf(lcd_putc, "AYARLAR");
               lcd_gotoxy(1,2);
               printf(lcd_putc, "Saat: %02d:%02d", time_hour, time_min);
               switch(settings_no){
                  case 1:
                     lcd_gotoxy(8,2);
                     break;
                  case 2:
                     lcd_gotoxy(11,2);
                     break;
                  default: 
                     settings_no = 1;
                     lcd_gotoxy(8,2);
                     break;
               }        
               break;
            case 2:
               lcd_putc("\f");
               lcd_gotoxy(1,1);
               printf(lcd_putc, "AYARLAR");
               lcd_gotoxy(1,2);
               printf(lcd_putc, "Tarih: %02d/%02d/%02d   ", date_day, date_mth, date_year);
               switch(settings_no){
                  case 1:
                     lcd_gotoxy(9,2);
                     break;
                  case 2:
                     lcd_gotoxy(12,2);
                     break;
                  case 3:
                     lcd_gotoxy(15,2);
                     break;
                  default: 
                     settings_no = 1;
                     lcd_gotoxy(9,2);
                     break;
               }      
               break;
            case 3:
               lcd_putc("\f");
               lcd_gotoxy(1,1);
               printf(lcd_putc, "AYARLAR");
               lcd_gotoxy(1,2);
               printf(lcd_putc, "PERIYOT: %2u SAAT", motor_period);
               switch(settings_no){
                  case 1:
                     lcd_gotoxy(11,2);
                     break;
                  default: 
                     settings_no = 1;
                     lcd_gotoxy(11,2);
                     break;
               } 
               break;
            case 4:
               lcd_putc("\f");
               lcd_gotoxy(1,1);
               printf(lcd_putc, "AYARLAR");
               lcd_gotoxy(1,2);
               printf(lcd_putc, "IDEAL: %2u   H: %1u", temp_ideal, temp_sensivity);
               switch(settings_no){
                  case 1:
                     lcd_gotoxy(9,2);
                     break;
                  case 2:
                     lcd_gotoxy(16,2);
                     break;
                  default: 
                     settings_no = 1;
                     lcd_gotoxy(9,2);
                     break;
               }              
               break;
            case 5:
               lcd_putc("\f");
               lcd_gotoxy(1,1);
               printf(lcd_putc, "HARICI TEST");
               lcd_gotoxy(1,2);
               printf(lcd_putc, "ISIT: %u SOGUT: %u", heater, cooler);
               switch(settings_no){
                  case 1:
                     lcd_gotoxy(7,2);
                     break;
                  case 2:
                     lcd_gotoxy(16,2);
                     break;
                  default: 
                     settings_no = 1;
                     lcd_gotoxy(7,2);
                     break;
               }               
               break;       
            case 6:
               lcd_putc("\f");
               lcd_gotoxy(1,1);
               printf(lcd_putc, "ADIMI SIFIRLA :)");
               lcd_gotoxy(1,2);
               printf(lcd_putc, "ADIM: %3u -> CHG", motor_cur_step );
               switch(settings_no){
                  case 1:
                     lcd_gotoxy(9,2);
                     break;
                  default: 
                     settings_no = 1;
                     lcd_gotoxy(9,2);
                     break;
               }               
               break;               
            default:
               lcd_putc("\f");
               lcd_gotoxy(1,1);
               printf(lcd_putc, "!!! HATA - MENU !!!");
               lcd_gotoxy(1,2);
               printf(lcd_putc, "                            ");
               break;
         }
      prev_screen = cur_screen_no;
      lcd_update = 0;
      }
   } 
}

void calc_cooler_heater() {
   if (temp < temp_ideal - temp_sensivity)
   // SICAKLIK (IDEAL-HASSASIYET) ALTINA DUSERSE, ISITMA EMRI GONDER
      heater = 1;
   else if (temp >= temp_ideal)
   // YOKSA ISITMAYI IPTAL ET
      heater = 0;
      
   if (temp > temp_ideal + temp_sensivity)
   // SICAKLIK (IDEAL+HASSASIYET) USTUNE CIKARSA, SOGUTMA EMRI GONDER 
      cooler = 1;      
   else if (temp <= temp_ideal)
   // YOKSA SOGUTMAYI IPTAL ET
      cooler = 0;
}

#INT_TIMER1
void read_int() { // Kesme Periyodu = 250ms

   // Saat degistirme ayarinda degilse -> Saati Oku
   if ( cur_screen_no != 1 ) {
      rtc_get_time(time_hour, time_min, time_sec);
   }
   
   // Tarih degistirme ayarinda degilse -> Tarihi Oku
   else if ( cur_screen_no != 2 ) {
      rtc_get_date(date_day, date_mth, date_year, date_dow);
   }
   
   // Sicakligi Oku
   temp = ds_read_temp();
   
   // Eger isitma sogutma test menusunde degilse -> Isitma, Sogutma Oku
   if ( cur_screen_no != 5 )
      calc_cooler_heater();   

   // ISITMA EMRI GELIRSE, LAMBAYI AC, YOKSA KAPAT
   if ( heater )
      output_high(LAMP_PIN); 
   else
      output_low(LAMP_PIN);
   
   // SOGUTMA EMRI GELIRSE, FANI AC, YOKSA KAPAT
   if ( cooler )
      output_high(FAN_PIN);
   else
      output_low(FAN_PIN);
      
   // Motorun donus vakti geldiyse, motoru belirtilen adim kadar dondur
   if ( time_hour == next_turn_hr ) {
      if ( time_min == next_turn_min ) {
         motor_move_relative(motor_step_count, 'F');
      }
   }
}

// PIC elektrik kesintisinde onceki bilgileri eeprom hafizasindan getirir.
void get_from_eeprom() { 
   // Periyod degerini getir
   motor_period = read_eeprom(0);
   if ( motor_period == 0xFF )   
      motor_period = 6;
      
   // Motorun adimini getir 
   motor_cur_step = read_eeprom(1);
   if ( motor_cur_step == 0xFF )
      motor_cur_step = 0;
      
   // Gelecek dondurme surelerini getir. ( saat / dakika )
   next_turn_hr = read_eeprom(2);
   next_turn_min = read_eeprom(3);
   
   if ( next_turn_hr == 0xFF )
      next_turn_hr = 0;
   
   if ( next_turn_min == 0xFF )
      next_turn_min = 0;   
   
   // Sicaklik Ideal ve Hassasiyet getir ( ideal / sensivity )
   temp_ideal = read_eeprom(4);
   temp_sensivity = read_eeprom(5);
   
   if ( temp_ideal == 0xFF )
      temp_ideal = 30;
      
   if ( temp_sensivity == 0xFF )
      temp_sensivity = 5;
}

void main ()
{
   lcd_init(); 
   rtc_init();
   delay_ms(20);
   //rtc_set_datetime(29,4,14,2,22,54);   // Varsayilan saat ayarlama;
     
   setup_adc(ADC_CLOCK_INTERNAL);
   setup_adc_ports(AN0);

   set_tris_a(0x1F);
   
   set_adc_channel(0);
   delay_us(20);
   
   get_from_eeprom();
   calculate_next_turn();
   
   setup_timer_0(RTCC_INTERNAL | RTCC_DIV_256); // timer0
   setup_timer_1(T1_INTERNAL | T1_DIV_BY_8); // timer1
   set_timer0(0); // TIMER0 65ms = (256*(2^8 - 12)*4) / 4MHZ
   set_timer1(34286); // TIMER1 250ms = (8*(2^16 - 34286)*4) / 4MHZ
   enable_interrupts(INT_EXT);
   enable_interrupts(INT_TIMER0);
   enable_interrupts(INT_TIMER1);
   enable_interrupts(GLOBAL);
 
   while(TRUE);
}
