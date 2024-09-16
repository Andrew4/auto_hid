/*
Credit:

Keyboard and Mouse Control
https://docs.arduino.cc/built-in-examples/usb/KeyboardAndMouseControl/

Modified Sep 16, 2024
by Kocsis Andor <andor_kocsis@yahoo.com>

License CC BY-SA 4.0
https://creativecommons.org/licenses/by-sa/4.0/
*/

#include "Keyboard.h"
#include "Mouse.h"

//--
//--App-specific Defines
//--

//--API version
#define def_api_version 0x01
//Serial input: chr_'+' cmd param (base16 8bit)
//Serial output: chr_'+' report param (base16 8bit)
//Example input: "+00", output: "+0001". Uselful to identify the api
//  version, or report back the idle state of the device.
//The application reads every op code from the serial port and so
//  executes them in that order. When you send the def_cmd_start_chr
//  (below), that also resets the input parser.
#define def_cmd_start_chr '+'
//--Serial input command codes
#define def_cmdcode_report_api 0x00
#define def_cmdcode_report_mcu_clock 0x01
//I am using this to detect stability problems. If the mcu
//  restarts, the clock resets. The output nibble order is
//  the same as the input: most significant first. The
//  output is a 64-bit value (8x 1 bytes), tells milliseconds.
//--
#define def_cmdcode_mouse_left_max 0x10
#define def_cmdcode_mouse_right_max 0x11
#define def_cmdcode_mouse_up_max 0x12
#define def_cmdcode_mouse_down_max 0x13
#define def_cmdcode_mouse_wheelup_one 0x14
#define def_cmdcode_mouse_wheeldown_one 0x15
#define def_cmdcode_mouse_left_param 0x16
#define def_cmdcode_mouse_right_param 0x17
#define def_cmdcode_mouse_up_param 0x18
#define def_cmdcode_mouse_down_param 0x19
#define def_cmdcode_mouse_wheelup_param 0x1a
#define def_cmdcode_mouse_wheeldown_param 0x1b
//mouse move +/- 127, wheel +/- 15
//--
#define def_cmdcode_button_click_left 0x20
#define def_cmdcode_button_click_mid 0x21
#define def_cmdcode_button_click_right 0x22
#define def_cmdcode_button_press_left 0x23
#define def_cmdcode_button_press_mid 0x24
#define def_cmdcode_button_press_right 0x25
#define def_cmdcode_button_release_left 0x26
#define def_cmdcode_button_release_mid 0x27
#define def_cmdcode_button_release_right 0x28
//--
#define def_cmdcode_key_release_all 0x30
#define def_cmdcode_key_click_param 0x31
#define def_cmdcode_key_press_param 0x32
#define def_cmdcode_key_release_param 0x33
//param=key code:
// https://www.arduino.cc/reference/en/language/functions/usb/
//     keyboard/keyboardmodifiers/
//--
#define def_cmdcode_delay_20ms 0x40
#define def_cmdcode_delay_80ms 0x41
//--Statemachine phases
#define def_cmdstm_reset 1
#define def_cmdstm_cmd_start_wait 2
#define def_cmdstm_cmd_read_high 3
#define def_cmdstm_cmd_read_low 4
#define def_cmdstm_param_read_high 5
#define def_cmdstm_param_read_low 6
#define def_cmdstm_cmd_exec 7

//--
//--Variables (I left them all in global)
//--

//--64-bit time recording and limits [millisec]
unsigned long time_h, time_l;
unsigned long time_limit_h, time_limit_l;
//--Conversion and general purpose vars
unsigned long t_ul;
char t_chr;
byte t_byte, t_byte2;
//--Serial input command and parameter
byte cmd_code;
byte cmd_param;
//--Statemachine phase
byte stm_ph;

//--
//--Arduino framework functions
//--

void setup() {
  //--
  stm_ph= def_cmdstm_reset;
  time_h= 0UL;
  time_l= 0UL;
  time_limit_h= 0UL;
  time_limit_l= 0UL;
  //--
  Serial.begin(9600);
  Mouse.begin();
  Keyboard.begin();
  //--
  pinMode(LED_BUILTIN_TX,INPUT);
  pinMode(LED_BUILTIN_RX,INPUT);
    //Turn off the blinking Serial port LEDs.
  //--
  return;}

void loop() {
  time_update();
  if (time_limit_block()) return;
  while(cmd_state_machine());
  return;}

//--
//--64-bit clock
//--

void time_update() {
  t_ul= millis();
  if (t_ul< time_l) time_h++;
  time_l= t_ul;
  return;}

bool time_limit_block() {
  if (time_limit_h >time_h) return true;
  if (time_limit_h< time_h) return false;
  if (time_limit_l >=time_l) return true;
  return false;}

void time_limit_set20ms() {
  time_limit_h= time_h;
  time_limit_l= time_l;
  time_limit_l+= 20UL;
  if (time_limit_l< time_l) time_limit_h++;
  return;}

void time_limit_set80ms() {
  time_limit_h= time_h;
  time_limit_l= time_l;
  time_limit_l+= 80UL;
  if (time_limit_l< time_l) time_limit_h++;
  return;}

//--
//--Serial communication and verification
//--

bool serial_read() {
  if (Serial.available()< 1) return false;
  t_chr= Serial.read();
  return true;}

void serial_send_start() {
  Serial.write(def_cmd_start_chr);
  return;}

void serial_send_byte_base16() {
  t_byte2= t_byte;
  t_byte= (t_byte2 >> 4);
  byte_convert();
  Serial.write(t_chr);
  t_byte= (t_byte2 & 0x0f);
  byte_convert();
  Serial.write(t_chr);
  return;}

bool chr_is_cmd_start() {
  if (t_chr==def_cmd_start_chr) return true;
  return false;}

bool cmd_is_valid() {
 if ((cmd_code >=def_cmdcode_report_api) &&
     (cmd_code<= def_cmdcode_report_mcu_clock)) return true;
 if ((cmd_code >=def_cmdcode_mouse_left_max) &&
     (cmd_code<= def_cmdcode_mouse_wheeldown_param)) return true;
 if ((cmd_code >=def_cmdcode_button_click_left) &&
     (cmd_code<= def_cmdcode_button_release_right)) return true;
 if ((cmd_code >=def_cmdcode_key_release_all) &&
     (cmd_code<= def_cmdcode_key_release_param)) return true;
 if ((cmd_code >=def_cmdcode_delay_20ms) &&
     (cmd_code<= def_cmdcode_delay_80ms)) return true;
 return false;}

bool cmd_param_req() {
 if ((cmd_code >=def_cmdcode_mouse_left_param) &&
     (cmd_code<= def_cmdcode_mouse_wheeldown_param)) return true;
 if ((cmd_code >=def_cmdcode_key_click_param) &&
     (cmd_code<= def_cmdcode_key_release_param)) return true;
 return false;}

//--
//--Base16 conversion
//--

bool chr_convert() {
  if ((t_chr >='0') && (t_chr<= '9')) {
    t_byte= (t_chr- '0');
    return true;}
  if ((t_chr >='a') && (t_chr<= 'f')) {
    t_byte= (t_chr- 'a'+ 0x0a);
    return true;}
  if ((t_chr >='A') && (t_chr<= 'F')) {
    t_byte= (t_chr- 'A'+ 0x0a);
    return true;}
  return false;}

void byte_convert() {
  if (t_byte<= 0x09) t_chr= ('0'+ t_byte);
  if (t_byte >=0x0a) t_chr= ('a'+ t_byte- 0x0a);
  return;}

//--
//--Statemachine to process the input from the serial port
//--

  //Returns true until it asks for extra calls
bool cmd_state_machine() {
  switch(stm_ph) {
    case def_cmdstm_reset:
      if (!Serial) return false;
      stm_ph= def_cmdstm_cmd_start_wait;
      return true;
    case def_cmdstm_cmd_start_wait:
      while(true) {
        if (!serial_read()) return false;
        if (!chr_is_cmd_start()) continue;
        stm_ph= def_cmdstm_cmd_read_high;
        break;}
      return true;
    case def_cmdstm_cmd_read_high:
      if (!serial_read()) return false;
      if (chr_is_cmd_start()) {
        stm_ph= def_cmdstm_cmd_read_high;
        return true;}
      if (!chr_convert()) {
        stm_ph= def_cmdstm_cmd_start_wait;
        return true;}
      cmd_code= (t_byte << 4);
      stm_ph= def_cmdstm_cmd_read_low;
      return true;
    case def_cmdstm_cmd_read_low:
      if (!serial_read()) return false;
      if (chr_is_cmd_start()) {
        stm_ph= def_cmdstm_cmd_read_high;
        return true;}
      if (!chr_convert()) {
        stm_ph= def_cmdstm_cmd_start_wait;
        return true;}
      cmd_code+= t_byte;
      if (!cmd_is_valid()) {
        stm_ph= def_cmdstm_cmd_start_wait;
        return true;}
      stm_ph= def_cmdstm_cmd_exec;
      if (cmd_param_req()) stm_ph= def_cmdstm_param_read_high;
      return true;
    case def_cmdstm_param_read_high:
      if (!serial_read()) return false;
      if (chr_is_cmd_start()) {
        stm_ph= def_cmdstm_cmd_read_high;
        return true;}
      if (!chr_convert()) {
        stm_ph= def_cmdstm_cmd_start_wait;
        return true;}
      cmd_param= (t_byte << 4);
      stm_ph= def_cmdstm_param_read_low;
      return true;
    case def_cmdstm_param_read_low:
      if (!serial_read()) return false;
      if (chr_is_cmd_start()) {
        stm_ph= def_cmdstm_cmd_read_high;
        return true;}
      if (!chr_convert()) {
        stm_ph= def_cmdstm_cmd_start_wait;
        return true;}
      cmd_param+= t_byte;
      stm_ph= def_cmdstm_cmd_exec;
      return true;
    case def_cmdstm_cmd_exec:
      cmd_exec();
      stm_ph= def_cmdstm_cmd_start_wait;
      return false;
    default:
      break;}
  return false;}

//--
//--Command execution (from the serial port)
//--

void cmd_exec() {
  switch(cmd_code) {
    case def_cmdcode_report_api:
      serial_send_start();
      t_byte= def_cmdcode_report_api;
      serial_send_byte_base16();
      t_byte= def_api_version;
      serial_send_byte_base16();
      break;
    case def_cmdcode_report_mcu_clock:
      serial_send_start();
      t_byte= def_cmdcode_report_mcu_clock;
      serial_send_byte_base16();
      t_byte= lowByte(time_h >> 24);
      serial_send_byte_base16();
      t_byte= lowByte(time_h >> 16);
      serial_send_byte_base16();
      t_byte= lowByte(time_h >> 8);
      serial_send_byte_base16();
      t_byte= lowByte(time_h);
      serial_send_byte_base16();
      t_byte= lowByte(time_l >> 24);
      serial_send_byte_base16();
      t_byte= lowByte(time_l >> 16);
      serial_send_byte_base16();
      t_byte= lowByte(time_l >> 8);
      serial_send_byte_base16();
      t_byte= lowByte(time_l);
      serial_send_byte_base16();
      break;
    case def_cmdcode_mouse_left_max:
      t_chr= (-127);
      Mouse.move(t_chr, 0, 0);
      break;
    case def_cmdcode_mouse_right_max:
      t_chr= 127;
      Mouse.move(t_chr, 0, 0);
      break;
    case def_cmdcode_mouse_up_max:
      t_chr= (-127);
      Mouse.move(0, t_chr, 0);
      break;
    case def_cmdcode_mouse_down_max:
      t_chr= 127;
      Mouse.move(0, t_chr, 0);
      break;
    case def_cmdcode_mouse_wheelup_one:
      t_chr= 1;
      Mouse.move(0, 0, t_chr);
      break;
    case def_cmdcode_mouse_wheeldown_one:
      t_chr= (-1);
      Mouse.move(0, 0, t_chr);
      break;
    case def_cmdcode_mouse_left_param:
      t_chr= ((cmd_param & 0x7f) * (-1));
      Mouse.move(t_chr, 0, 0);
      break;
    case def_cmdcode_mouse_right_param:
      t_chr= (cmd_param & 0x7f);
      Mouse.move(t_chr, 0, 0);
      break;
    case def_cmdcode_mouse_up_param:
      t_chr= ((cmd_param & 0x7f) * (-1));
      Mouse.move(0, t_chr, 0);
      break;
    case def_cmdcode_mouse_down_param:
      t_chr= (cmd_param & 0x7f);
      Mouse.move(0, t_chr, 0);
      break;
    case def_cmdcode_mouse_wheelup_param:
      t_chr= (cmd_param & 0x0f);
      Mouse.move(0, 0, t_chr);
      break;
    case def_cmdcode_mouse_wheeldown_param:
      t_chr= ((cmd_param & 0x0f) * (-1));
      Mouse.move(0, 0, t_chr);
      break;
    case def_cmdcode_button_click_left:
      Mouse.click(MOUSE_LEFT);
      break;
    case def_cmdcode_button_click_mid:
      Mouse.click(MOUSE_MIDDLE);
      break;
    case def_cmdcode_button_click_right:
      Mouse.click(MOUSE_RIGHT);
      break;
    case def_cmdcode_button_press_left:
      Mouse.press(MOUSE_LEFT);
      break;
    case def_cmdcode_button_press_mid:
      Mouse.press(MOUSE_MIDDLE);
      break;
    case def_cmdcode_button_press_right:
      Mouse.press(MOUSE_RIGHT);
      break;
    case def_cmdcode_button_release_left:
      Mouse.release(MOUSE_LEFT);
      break;
    case def_cmdcode_button_release_mid:
      Mouse.release(MOUSE_MIDDLE);
      break;
    case def_cmdcode_button_release_right:
      Mouse.release(MOUSE_RIGHT);
      break;
    case def_cmdcode_key_release_all:
      Keyboard.releaseAll();
      break;
    case def_cmdcode_key_click_param:
      t_chr= cmd_param;
      Keyboard.write(t_chr);
      break;
    case def_cmdcode_key_press_param:
      t_chr= cmd_param;
      Keyboard.press(t_chr);
      break;
    case def_cmdcode_key_release_param:
      t_chr= cmd_param;
      Keyboard.release(t_chr);
      break;
    case def_cmdcode_delay_20ms:
      time_limit_set20ms();
      break;
    case def_cmdcode_delay_80ms:
      time_limit_set80ms();
      break;
    default:
      break;}
  return false;}












