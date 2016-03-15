// http://stackoverflow.com/questions/17762763/play-wav-sound-file-encoded-in-base64-with-javascript
#define ArrayLen(x)  (sizeof(x) / sizeof((x)[0]))

#include "main.h"
Window *main_window;
Layer *main_layer, *root_layer, *header_layer;
#define header_height IF_CHALK_ELSE(40, 26)
GColor header_color;

char time_text[] = "00:00  ";

bool js_connected=false;
bool phone_connected=false;
uint8_t phone_battery_level=255;
bool phone_charging=false;
uint8_t watch_battery_level = 255;
bool watch_charging=false;

void create_menu();

void dirty() {if(main_window) if(window_get_root_layer(main_window)) layer_mark_dirty(window_get_root_layer(main_window));}
bool emulator=false;
char buffer[256];

uint8_t cursor=3;
uint8_t cursor_max = 0;//5;
uint8_t cursor_max_hold = 99;

char dictation_text[100] = "\n\nConnecting to phone...";  // Voice API has a 100 character limit.
//char dictation_text[100] = "Welcome to Android Voice Relay for Pebble. This is a very long string to test chalk";  // Voice API has a 100 character limit.
//char dictation_text[100] = "testing testing one two three";  // Voice API has a 100 character limit.
char welcome_message[] = "Welcome to Android Voice Relay for Pebble";

 const char * const phrases[] = {
   "Hello There!",
   "Testing.  Testing.  1, 2, 3.",
   "Pebble Babble Bubble Nibble Quibble",
   "Why i oughta...",
   "Where is my mummy?",
   "This is what you get when you fight a stranger in the Alps!",
   "Next time, get a watch with a microphone",
   "Be vewy vewy quiet.  I'm hunting wabbits.",
   "I love you!",
   "Good Night"
 };
#define phraseslength ArrayLen(phrases)
uint8_t phrase = phraseslength;

enum {
  KEY_SYS     = 0,  // Recv: Receive system message
  KEY_BEEP    = 1,  // Send: Make a beep noise
  KEY_SPEAK   = 2,  // Send: Text-To-Speech
  KEY_VOICES  = 3,  // Send: Change Voice
  KEY_OPTIONS = 4,  // Send: Change Options such as pitch, rate and volume
  KEY_BATT    = 5,  // Recv: Phone battery message
  KEY_PITCH   = 6,
  KEY_SPEED   = 7,
  KEY_ERR     = 99  // Error!
//   KEY_PING = 5,          // Command: Ping to get delay (in ms) between send msg and recv msg to determine phone-watch distance
};

uint8_t voice_choice=0;
char *voices[] = {
"US English Male",
"US English Female",
"UK English Male",
"UK English Female",
"Afrikaans Male",
"Albanian Male",
"Arabic Male",
"Armenian Male",
"Australian Female",
"Bosnian Male",
"Brazilian Portuguese Female",
"Catalan Male",
"Chinese Female",
"Croatian Male",
"Czech Female",
"Czech Male",
"Danish Female",
"Danish Male",
"Deutsch Female",
"Dutch Female",
"Esperanto Male",
"Finnish Female",
"Finnish Male",
"French Female",
"Greek Female",
"Greek Male",
"Hatian Creole Female",
"Hindi Female",
"Hungarian Female",
"Hungarian Male",
"Icelandic Male",
"Indonesian Female",
"Italian Female",
"Japanese Female",
"Korean Female",
"Latin Female",
"Latin Male",
"Latvian Male",
"Macedonian Male",
"Moldavian Male",
"Montenegrin Male",
"Norwegian Female",
"Norwegian Male",
"Polish Female",
"Portuguese Female",
"Romanian Male",
"Russian Female",
"Serbian Male",
"Serbo-Croatian Male",
"Slovak Female",
"Slovak Male",
"Spanish Female",
"Spanish Latin American Female",
"Swahili Male",
"Swedish Female",
"Swedish Male",
"Tamil Male",
"Thai Female",
"Turkish Female",
"Vietnamese Male",
"Welsh Male"
};
uint8_t voice_male_female[] = {0,1,0,1,0,0,0,0,1,0,1,0,1,0,1,0,1,0,1,1,0,1,0,1,1,0,1,1,1,0,0,1,1,1,1,1,0,0,0,0,0,1,0,1,1,0,1,0,0,1,0,1,1,0,1,0,0,1,1,0,0};
  
char *speedtext[5]={"0", "0.5", "1", "1.5", "2"};
uint8_t speed = 2;
char *pitchtext[3]={"0", "1", "2"};
uint8_t pitch = 1;

uint8_t beepmode = 0;


// ********************************************************************************* //
//  Pebble Phone Communication
// ********************************************************************************* //
char *translate_dictionaryresult(DictionaryResult result) {
  switch (result) {
    case DICT_OK: return "DICT_OK";
    case DICT_NOT_ENOUGH_STORAGE: return "DICT_NOT_ENOUGH_STORAGE";
    case DICT_INVALID_ARGS: return "DICT_INVALID_ARGS";
    case DICT_INTERNAL_INCONSISTENCY: return "DICT_INTERNAL_INCONSISTENCY";
    case DICT_MALLOC_FAILED: return "DICT_MALLOC_FAILED";
    default: return "UNKNOWN ERROR";
  }
}

char *translate_appmessageresult(AppMessageResult result) {
  switch (result) {
    case APP_MSG_OK: return "APP_MSG_OK";
    case APP_MSG_SEND_TIMEOUT: return "APP_MSG_SEND_TIMEOUT";
    case APP_MSG_SEND_REJECTED: return "APP_MSG_SEND_REJECTED";
    case APP_MSG_NOT_CONNECTED: return "APP_MSG_NOT_CONNECTED";
    case APP_MSG_APP_NOT_RUNNING: return "APP_MSG_APP_NOT_RUNNING";
    case APP_MSG_INVALID_ARGS: return "APP_MSG_INVALID_ARGS";
    case APP_MSG_BUSY: return "APP_MSG_BUSY";
    case APP_MSG_BUFFER_OVERFLOW: return "APP_MSG_BUFFER_OVERFLOW";
    case APP_MSG_ALREADY_RELEASED: return "APP_MSG_ALREADY_RELEASED";
    case APP_MSG_CALLBACK_ALREADY_REGISTERED: return "APP_MSG_CALLBACK_ALREADY_REGISTERED";
    case APP_MSG_CALLBACK_NOT_REGISTERED: return "APP_MSG_CALLBACK_NOT_REGISTERED";
    case APP_MSG_OUT_OF_MEMORY: return "APP_MSG_OUT_OF_MEMORY";
    case APP_MSG_CLOSED: return "APP_MSG_CLOSED";
    case APP_MSG_INTERNAL_ERROR: return "APP_MSG_INTERNAL_ERROR";
    default: return "UNKNOWN ERROR";
  }
}

void send_message_to_phone(uint32_t key, char *str) {
  DictionaryIterator *iter;
  uint32_t result = (uint32_t)app_message_outbox_begin(&iter);
  if(result !=APP_MSG_OK) {
    LOG("Error %s returned from app_message_outbox_begin (message key %d): %s", translate_appmessageresult(result), (int)key, str);
  } else if (!iter) {
    LOG("Error creating iterator for the outbound message. (message key %d): %s", (int)key, str);
  } else {
    result = dict_write_cstring(iter, key, str);
    if(result != DICT_OK) {
      LOG("Error %s returned from dict_write_cstring (message key %d): %s", translate_dictionaryresult(result), (int)key, str);
    } else {
      result = dict_write_end(iter);
      if(result==0) {
        LOG("Dictionary size %d bytes returned from dict_write_end (message key %d): %s", (int)result, (int)key, str);
      } else {
        AppMessageResult result = app_message_outbox_send();
        if(result !=APP_MSG_OK) {
          LOG("Error %s returned from app_message_outbox_send (message key %d): %s", translate_appmessageresult(result), (int)key, str);
        }
      }
    }
  }
}

void send_message_to_phone_old(uint32_t key, char *str) {
  DictionaryIterator *iter;
  uint32_t result = (uint32_t)app_message_outbox_begin(&iter);
  if(result !=APP_MSG_OK) {
    LOG("Error %d returned from app_message_outbox_begin (message key %d): %s", (int)result, (int)key, str);
  } else if (!iter) {
    LOG("Error creating iterator for the outbound message. (message key %d): %s", (int)key, str);
  } else {
    result = dict_write_cstring(iter, key, str);
    if(result != DICT_OK) {
      LOG("Error %d returned from dict_write_cstring (message key %d): %s", (int)result, (int)key, str);
    } else {
      result = dict_write_end(iter);
      if(result==0) {
        LOG("Dictionary size %d bytes returned from dict_write_end (message key %d): %s", (int)result, (int)key, str);
      } else {
        AppMessageResult result = app_message_outbox_send();
        if(result !=APP_MSG_OK) {
          LOG("Error %d returned from app_message_outbox_send (message key %d): %s", (int)result, (int)key, str);
        }
      }
    }
  }
}

static void timer_callback(void *data) {
  if(cursor_max_hold > 0 && cursor_max_hold < 99) {
    strcpy(dictation_text, welcome_message);
    cursor_max = cursor_max_hold;//(int)data;
    cursor_max_hold = 0;
    dirty();
  }
}

void process_tuple(Tuple *t) {
            int16_t level;
char string_value[256]="0000";
  int key = t->key;
  strcpy(string_value, t->value->cstring);  //Get string value, if present
  //snprintf(buffer, sizeof(buffer), "Message %d: %s", key, string_value);  LOG(buffer);  // Log whatever it is
  
  switch(key) {
    case KEY_SYS:  // System Message
      if(string_value[0]=='S') { // Apple
        cursor_max = 0;
        cursor_max_hold = 0;
        strcpy(dictation_text, string_value);
      } else if(string_value[0]=='X') { // No Beeps
        strcpy(dictation_text, &string_value[1]);  // Remove first character of message and display
        cursor_max_hold = 4;  //cursor_max = 4;
        app_timer_register(5000, timer_callback, (void*)4); // Display Detected Phone then wait a moment
      } else if(string_value[0]=='C') {  // Good
        strcpy(dictation_text, &string_value[1]);  // Remove first character of message and display
        app_timer_register(5000, timer_callback, (void*)5); // Display Detected Phone then wait a moment
        cursor_max_hold = 5;  //cursor_max = 5;
      } else {
        //snprintf(buffer, sizeof(buffer), "System Message: %s", string_value); LOG("%s", buffer);
        strcpy(dictation_text, string_value);
      }
    break;
    case KEY_BATT:
      snprintf(buffer, sizeof(buffer), "Battery Message: %s", string_value); LOG("%s", buffer);
      switch(string_value[0]) {
        case 'C':
          switch(string_value[1]) {
            case 'Y':
              phone_charging = true;
            break;
            case 'N':
              phone_charging = false;
            break;
          }
        break;
        case 'L':
          level = 0;
          level = level + (100*((uint16_t)(string_value[1])-48));
          level = level + (10* ((uint16_t)(string_value[2])-48));
          level = level + (1*  ((uint16_t)(string_value[3])-48));
        LOG("L: %d - %d - %d", (100*(uint16_t)(string_value[1]-48)), (10*(uint16_t)(string_value[2]-48)), (1*(uint16_t)(string_value[3]-48)));
          if(level>100 || level<0)
            phone_battery_level = 255;  // bad battery level
          else
            phone_battery_level = (uint8_t)level;
        
        LOG("New PhoneBatt: %d %d", phone_battery_level, level);
        break;
      }
    break;
    case KEY_ERR:  // Error
    snprintf(buffer, sizeof(buffer), "Error Message: %s", string_value); LOG("%s", buffer);
    break;
  }
}

void inbox_received_callback(DictionaryIterator *iter, void *context) {
  Tuple *t = dict_read_first(iter);
  while(t != NULL) {
    process_tuple(t);
    t = dict_read_next(iter);
  }
  js_connected=true;
  dirty();
}

void inbox_dropped_callback(AppMessageResult reason, void *context) {
  LOG("Phone-to-Pebble message dropped! Reason: %d", reason);
  js_connected=false;
}

void outbox_failed_callback(DictionaryIterator *iterator, AppMessageResult reason, void *context) {
  LOG("Pebble-to-phone message send failed!  Reason: %d", reason);
  js_connected=false;
}

void outbox_sent_callback(DictionaryIterator *iterator, void *context) {
  //LOG("Pebble-to-phone message succees!");
  js_connected=true;
}

// ********************************************************************************* //
//  Send Audio Communication to JS
// ********************************************************************************* //

void beep(char *beep_message) {
  send_message_to_phone(KEY_BEEP, beep_message);
}

void change_voice(char *voice) {
    send_message_to_phone(KEY_VOICES, voice);
}

void change_rate_pitch_volume() {
  
}

void speak(char *words) {
  DictionaryIterator *iter;
  uint32_t result = (uint32_t)app_message_outbox_begin(&iter);
  if(result !=APP_MSG_OK) {
    LOG("Error %s returned from app_message_outbox_begin", translate_appmessageresult(result));
    return;
  }
  if (!iter) {
    LOG("Error creating iterator for the outbound message.");
    return;
  }
  result = dict_write_cstring(iter, KEY_SPEED, speedtext[speed]);
  if(result != DICT_OK) {
    LOG("Error %s returned from dict_write_cstring.", translate_dictionaryresult(result));
    return;
  }
  result = dict_write_cstring(iter, KEY_PITCH, pitchtext[pitch]);
  if(result != DICT_OK) {
    LOG("Error %s returned from dict_write_cstring.", translate_dictionaryresult(result));
    return;
  }
  result = dict_write_cstring(iter, KEY_SPEAK, words);
  if(result != DICT_OK) {
    LOG("Error %s returned from dict_write_cstring.", translate_dictionaryresult(result));
    return;
  }
  result = dict_write_end(iter);
  if(result==0) {
    LOG("Dictionary size %d bytes returned from dict_write_end.", (int)result);
    return;
  }
  //AppMessageResult result2 = ;
  if(app_message_outbox_send() !=APP_MSG_OK)
    LOG("Error %s returned from app_message_outbox_send.", translate_appmessageresult(result));
}
// ===============================================================================================================================================
//  Dictation API
// ===============================================================================================================================================
DictationSession *dictation_session = NULL;
#ifdef PBL_SDK_3
static void dictation_session_callback(DictationSession *session, DictationSessionStatus status, char *transcription, void *context) {
  if(status == DictationSessionStatusSuccess) {
    // Send the dictated text
    snprintf(dictation_text, sizeof(dictation_text), "%s", transcription);
    LOG("Sending Dictation Text: %s", dictation_text);
    speak(dictation_text);
  } else {
    // Display the reason for any error
    //snprintf(dictation_text, sizeof(dictation_text), "Transcription failed.\nError ID: %d", (int)status);
    snprintf(dictation_text, sizeof(dictation_text), "Dictation Err: %d", (int)status);
    LOG("%s", dictation_text);
    //add_text(dictation_text, 0b11000000, 0b11110000);  // error message to screen
  }
}
#endif


void set_text(char *str, uint8_t text_color, uint8_t bgcolor) {
  
}


// ===============================================================================================================================================
//  Button Functions
// ===============================================================================================================================================
void sl_click_handler  (ClickRecognizerRef recognizer, void *context) { // SELECT button
  if(cursor_max_hold > 0) {
    cursor_max_hold = 5;
    timer_callback(NULL);
    return;
  }
  if(cursor_max > 0) {  // If screen has something
    switch(cursor) {
      case 0:
        create_menu();
      break;

      case 1:
        speed++;
        if(speed>4)
          speed=0;
        send_message_to_phone(KEY_SPEED, speedtext[speed]);
      break;

      case 2:
        pitch++;
        if(pitch>2)
          pitch=0;
        send_message_to_phone(KEY_PITCH, pitchtext[pitch]);
      break;

      case 3:
        speak(dictation_text);
      break;

      case 4:
        #ifdef PBL_PLATFORM_APLITE
          //Aplite has no microphone, so change text in rotation
          phrase += 1;
          if(phrase >= phraseslength)
            phrase = 0;
          strcpy(dictation_text, phrases[phrase]);
        #else
         /*
          phrase += 1;
          if(phrase >= phraseslength)
            phrase = 0;
          strcpy(dictation_text, phrases[phrase]);
          */
          dictation_session_start(dictation_session);
        #endif
      break;

      case 5:
        beepmode = (beepmode + 1)%4;
        if(beepmode == 0 || beepmode==2) {
           APP_LOG(APP_LOG_LEVEL_DEBUG, "Stop Modes in C");
          beep("beep stop");  // Stop beeping
        } else if(beepmode == 3) {
          beep("sonar pulses");  // Start beeping
          APP_LOG(APP_LOG_LEVEL_DEBUG, "Start sonar in C");
        } else if(beepmode == 1) {
          beep("annoying alarm");  // Start annoying beeping
          APP_LOG(APP_LOG_LEVEL_DEBUG, "Start alarm in C");
        }
      break;

    }
    dirty();
  }
}

void up_click_handler  (ClickRecognizerRef recognizer, void *context) { //   UP   button
  if(cursor_max_hold > 0) {
    timer_callback(NULL);
    return;
  }

  cursor--;
  if(cursor>cursor_max)  // uint8_t means below 0 becomes 255
    cursor = cursor_max;
  dirty();
}

void dn_click_handler  (ClickRecognizerRef recognizer, void *context) { //  DOWN  button
  if(cursor_max_hold > 0) {
    timer_callback(NULL);
    return;
  }

  cursor++;
  if(cursor>cursor_max)
    cursor=0;
  dirty();
}

void bk_click_handler  (ClickRecognizerRef recognizer, void *context) { //  BACK  button
  if(cursor_max_hold > 0 && cursor_max_hold <99) {
    timer_callback(NULL);
    return;
  }
  
  window_stack_pop_all(false);
}

void click_config_provider(void *context) {
  window_single_click_subscribe(BUTTON_ID_UP,     up_click_handler);
  window_single_click_subscribe(BUTTON_ID_DOWN,   dn_click_handler);
  window_single_click_subscribe(BUTTON_ID_SELECT, sl_click_handler);
  window_single_click_subscribe(BUTTON_ID_BACK,   bk_click_handler);
}

// ===============================================================================================================================================
//  Drawing Functions
// ===============================================================================================================================================
void main_layer_update(Layer *me, GContext *ctx) {
  #define FGColor IF_BW_COLOR(GColorBlack, GColorWhite)
  #define BGColor IF_BW_COLOR(GColorWhite, GColorDukeBlue)
  char text_to_draw[100];
  
  // Black or Blue Background
  graphics_context_set_fill_color(ctx, BGColor);
  graphics_fill_rect(ctx, layer_get_bounds(me), 0, GCornerNone);
  
  //uint8_t *framebuffer = (uint8_t*)*(size_t*)ctx;
  
  GSize size = layer_get_frame(me).size;

  
  GRect rect;// = GRect(IF_CHALK_ELSE(32, 16), IF_CHALK_ELSE(32, 0), size.w-16, 16);

  if(cursor_max == 0) {
    rect=GRect(16, 20, size.w - 32, size.h - 20);
    graphics_context_set_text_color(ctx, (cursor==1)?BGColor:FGColor);
    graphics_context_set_fill_color(ctx, (cursor==1)?FGColor:BGColor);
    graphics_fill_rect(ctx, rect, 0, GCornerNone);
    graphics_draw_text(ctx, dictation_text, fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD), rect, GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);
    // Prints Full-screen Message
  } else {
    rect = GRect(0, 4, size.w, 16);
    graphics_context_set_text_color(ctx, (cursor==0)?BGColor:FGColor);
    graphics_context_set_fill_color(ctx, (cursor==0)?FGColor:BGColor);
    graphics_fill_rect(ctx, rect, 0, GCornerNone);
    graphics_draw_text(ctx, voices[voice_choice], fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD), rect, GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
    rect.origin.y += 17;

    rect = GRect((size.w/2)-65, rect.origin.y, 60, 17);
    graphics_context_set_text_color(ctx, (cursor==1)?BGColor:FGColor);
    graphics_context_set_fill_color(ctx, (cursor==1)?FGColor:BGColor);
    graphics_fill_rect(ctx, rect, 0, GCornerNone);
    snprintf(text_to_draw, sizeof(text_to_draw), "Speed: %s", speedtext[speed]);
    graphics_draw_text(ctx, text_to_draw, fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD), rect, GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);

    rect=GRect((size.w/2)+15, rect.origin.y, 55, 17);
    graphics_context_set_text_color(ctx, (cursor==2)?BGColor:FGColor);
    graphics_context_set_fill_color(ctx, (cursor==2)?FGColor:BGColor);
    graphics_fill_rect(ctx, rect, 0, GCornerNone);
    snprintf(text_to_draw, sizeof(text_to_draw), "Pitch: %s", pitchtext[pitch]);
    graphics_draw_text(ctx, text_to_draw, fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD), rect, GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);

    rect.origin.y += 17;
    rect=GRect((size.w/2) - 32, rect.origin.y, 64, 17);
    graphics_context_set_text_color(ctx, (cursor==3)?BGColor:FGColor);
    graphics_context_set_fill_color(ctx, (cursor==3)?FGColor:BGColor);
    graphics_fill_rect(ctx, rect, 0, GCornerNone);
    graphics_draw_text(ctx, "[SPEAK]", fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD), rect, GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);

    rect.origin.y += 17;
    rect=GRect(16, rect.origin.y, size.w - 32, 60);
    graphics_context_set_text_color(ctx, (cursor==4)?BGColor:FGColor);
    graphics_context_set_fill_color(ctx, (cursor==4)?FGColor:BGColor);
    graphics_fill_rect(ctx, rect, 0, GCornerNone);
    graphics_draw_text(ctx, dictation_text, fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD), rect, GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);

    if(cursor_max == 5) {
      rect.origin.y += 60;
      rect=GRect((size.w/2) - 32, rect.origin.y, 64, 17);
      graphics_context_set_text_color(ctx, (cursor==5)?BGColor:FGColor);
      graphics_context_set_fill_color(ctx, (cursor==5)?FGColor:BGColor);
      graphics_fill_rect(ctx, rect, 0, GCornerNone);
      graphics_draw_text(ctx, "[BEEP]", fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD), rect, GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);
    }
  }
  //graphics_draw_text(ctx, mainmenustring, fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD), GRect(IF_CHALK_ELSE(32, 16), IF_CHALK_ELSE(32, 0), size.w-16, size.h), GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
    
  //graphics_context_set_stroke_color(ctx, GColorWhite);
  //graphics_draw_rect(ctx, GRect(20,20,20,20));
  
  //draw_screen_text(framebuffer);
}

void header_draw_update(Layer *me, GContext *ctx) {
  char text_to_draw[100];
  
  GRect frame = layer_get_frame(me);
  graphics_context_set_fill_color(ctx, IF_BW_COLOR(GColorBlack, phone_connected ? js_connected ? GColorGreen:GColorYellow:GColorRed));
  graphics_fill_rect(ctx, frame, 0, GCornerNone);
  #if defined(PBL_PLATFORM_CHALK)
  #define headertextheight 38  // Note: This is different than the header height since Chalk was cutting off the top corners
  
  graphics_context_set_text_color(ctx, GColorBlack);
  
  snprintf(text_to_draw, sizeof(text_to_draw), "Phn");
  graphics_draw_text(ctx, text_to_draw, fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD), GRect(frame.origin.x + 40, frame.origin.y + (frame.size.h - 30), frame.size.w, frame.size.h), GTextOverflowModeWordWrap, GTextAlignmentLeft, NULL);

  if(phone_battery_level>100 || !phone_connected || !js_connected) {
    snprintf(text_to_draw, sizeof(text_to_draw), "%s", phone_connected?js_connected?"Connected":"No Javascript":"Disconnected");
    LOG("Draw PhoneBatt: %d %c", phone_battery_level, phone_charging);
  } else {
    snprintf(text_to_draw, sizeof(text_to_draw), " %d%%%c", phone_battery_level, phone_charging ? '+' : '\0');
  }
  graphics_draw_text(ctx, text_to_draw, fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD), GRect(frame.origin.x + 24, frame.origin.y + (frame.size.h - 15), frame.size.w, frame.size.h), GTextOverflowModeWordWrap, GTextAlignmentLeft, NULL);
  
  //snprintf(text_to_draw, sizeof(text_to_draw), "%d%%%c:Peb", watch_battery_level, watch_charging ? '+' : ' ');
  //graphics_draw_text(ctx, text_to_draw, fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD), GRect(frame.origin.x, frame.origin.y + (frame.size.h - 15), frame.size.w-25, frame.size.h), GTextOverflowModeWordWrap, GTextAlignmentRight, NULL);
  if(watch_battery_level>100)
    snprintf(text_to_draw, sizeof(text_to_draw), "Peb\nERR");
  else
    snprintf(text_to_draw, sizeof(text_to_draw), "Peb\n%d%%%c", watch_battery_level, watch_charging ? '+' : '\0');
  graphics_draw_text(ctx, text_to_draw, fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD), GRect(frame.origin.x + (frame.size.w - 72), frame.origin.y + (frame.size.h - 30), 40, frame.size.h), GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);

//   graphics_draw_text(ctx, time_text,  fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD), GRect(frame.origin.x, frame.origin.y + (frame.size.h - 42), frame.size.w, frame.size.h), GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);
  
  
  
  ////graphics_draw_text(ctx, text_to_draw, fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD), GRect(frame.origin.x + 24, frame.origin.y + (frame.size.h - 15), frame.size.w, frame.size.h), GTextOverflowModeWordWrap, GTextAlignmentLeft, NULL);
//   graphics_draw_text(ctx, text_to_draw, fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD), GRect(frame.origin.x + 24, frame.origin.y + (frame.size.h - 30), 40, frame.size.h), GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);
  
//   snprintf(text_to_draw, sizeof(text_to_draw), "Peb\n%d%%%c", watch_battery_level, watch_charging ? '+' : '\0');
//   //graphics_draw_text(ctx, text_to_draw, fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD), GRect(frame.origin.x, frame.origin.y + (frame.size.h - 15), frame.size.w-25, frame.size.h), GTextOverflowModeWordWrap, GTextAlignmentRight, NULL);
//   graphics_draw_text(ctx, text_to_draw, fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD), GRect(frame.origin.x + (frame.size.w - 72), frame.origin.y + (frame.size.h - 30), 40, frame.size.h), GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);
  
  
  
  graphics_draw_text(ctx, time_text,  fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD), GRect(frame.origin.x, frame.origin.y + (frame.size.h - 42), frame.size.w, frame.size.h), GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);
  
  
  #else
    graphics_context_set_text_color(ctx, IF_BW_COLOR(GColorWhite, GColorBlack));
    
    //if(phone_battery_level>100 || !phone_connected) {
    if(phone_battery_level>100 || !phone_connected || !js_connected) {
      snprintf(text_to_draw, sizeof(text_to_draw), "Phone\n%s", phone_connected?js_connected?"Conn":"No JS":"Discon");
      //snprintf(text_to_draw, sizeof(text_to_draw), "Phone\n%s", phone_connected?"Conn":"Discon");
      //LOG("Draw PhoneBatt: %d %c", phone_battery_level, phone_charging);
    } else {
      snprintf(text_to_draw, sizeof(text_to_draw), "Phone\n%d%%%c", phone_battery_level, phone_charging ? '+' : '\0');
    }

    graphics_draw_text(ctx, text_to_draw, fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD), GRect(frame.origin.x + 2, frame.origin.y + (frame.size.h - 30), 40, frame.size.h), GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);

    if(watch_battery_level>100)
      snprintf(text_to_draw, sizeof(text_to_draw), "Watch\nERR");
    else
      snprintf(text_to_draw, sizeof(text_to_draw), "Watch\n%d%%%c", watch_battery_level, watch_charging ? '+' : '\0');

    graphics_draw_text(ctx, text_to_draw, fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD), GRect(frame.origin.x + (frame.size.w - 42), frame.origin.y + (frame.size.h - 30), 40, frame.size.h), GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);

    graphics_draw_text(ctx, time_text,  fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD), GRect(frame.origin.x, frame.origin.y + (frame.size.h - 32), frame.size.w, frame.size.h), GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);
  #endif
}

// ===============================================================================================================================================
//  Voice Selection Menu
// ===============================================================================================================================================
static GBitmap *male_icon, *female_icon;
MenuLayer *menu_layer;
Layer *menu_header_layer;
  
void destroy_menu(uint16_t result) {
  if(result != 65535) {
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Set voice from %d (%s) to %d (%s)", voice_choice, voices[voice_choice], result, voices[result]);
    voice_choice = result;
    send_message_to_phone(KEY_VOICES, voices[voice_choice]);
  } else {
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Canceled choosing voice");
  }
  
  if(menu_layer) menu_layer_destroy(menu_layer);
  if(menu_header_layer) layer_destroy(menu_header_layer);
  if(female_icon) gbitmap_destroy(female_icon);
  if(male_icon) gbitmap_destroy(male_icon);
  window_set_click_config_provider(main_window, click_config_provider);
  layer_add_child(root_layer, main_layer);
}

//-----------------------------------------------------------------
// Standard Menu Callbacks
//-----------------------------------------------------------------
uint16_t menu_get_num_rows_callback(MenuLayer *menu_layer, uint16_t section_index, void *data) {return sizeof(voices) / sizeof(voices[0]);}
int16_t menu_get_header_height_callback(MenuLayer *menu_layer, uint16_t section_index, void *data) {return 0;} // 0 height because there is no menu_header
int16_t menu_get_cell_height_callback(MenuLayer *menu_layer, MenuIndex *cell_index, void *callback_context) {return MENU_CELL_BASIC_HEADER_HEIGHT;}

//-----------------------------------------------------------------
// Menu Drawing Functions
//-----------------------------------------------------------------
static void menu_draw_row_callback(GContext* ctx, const Layer *cell_layer, MenuIndex *cell_index, void *data) {
  menu_cell_basic_header_draw(ctx, cell_layer, " ");  // Putting here to set the proper cell drawing attributes on Aplite.
  GSize size = layer_get_frame(cell_layer).size;
  graphics_draw_text(ctx, voices[cell_index->row], fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD), GRect(IF_CHALK_ELSE(32, 16), 0, size.w-16, size.h), GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
  graphics_draw_bitmap_in_rect(ctx, voice_male_female[cell_index->row] ? female_icon : male_icon, GRect(IF_CHALK_ELSE(16, 0),0,16,16));
}

void menu_header_draw_update(Layer *me, GContext *ctx) {
  #define menuheadertextheight 28  // Note: This is different than the header height since Chalk was cutting off the top corners
  GRect frame = layer_get_frame(me);
  graphics_context_set_fill_color(ctx, header_color);
  graphics_fill_rect(ctx, frame, 0, GCornerNone);
  
  frame.origin.y += (frame.size.h - menuheadertextheight);
  frame.size.h = menuheadertextheight;
  graphics_context_set_text_color(ctx, IF_BW_COLOR(GColorWhite, GColorBlack));
  graphics_draw_text(ctx, "Select a Voice", fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD), frame, GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);
}

//-----------------------------------------------------------------
// Menu Button Handling Functions
//-----------------------------------------------------------------
void menu_back_button_handler(ClickRecognizerRef recognizer, void *context) {
 destroy_menu(65535);
}
void menu_up_button_handler(ClickRecognizerRef recognizer, void *context) {
 MenuIndex index = menu_layer_get_selected_index(menu_layer);
  if(index.row>0) index.row--;
 menu_layer_set_selected_index(menu_layer, index, MenuRowAlignCenter, false);
}

void menu_down_button_handler(ClickRecognizerRef recognizer, void *context) {
  MenuIndex index = menu_layer_get_selected_index(menu_layer);
  if(index.row<(sizeof(voices) / sizeof(voices[0]))) index.row++;
  menu_layer_set_selected_index(menu_layer, index, MenuRowAlignCenter, false);
}

void menu_select_button_handler(ClickRecognizerRef recognizer, void *context) {
  MenuIndex index = menu_layer_get_selected_index(menu_layer);
  destroy_menu(index.row);
}

void menu_ccp(void *context) {
  window_single_click_subscribe(BUTTON_ID_BACK,   menu_back_button_handler);
  window_single_repeating_click_subscribe(BUTTON_ID_DOWN,   50, menu_down_button_handler);
  window_single_repeating_click_subscribe(BUTTON_ID_UP,     50, menu_up_button_handler);
  window_single_click_subscribe(BUTTON_ID_SELECT, menu_select_button_handler);
}
// End Menu Button Support

//-----------------------------------------------------------------
// Create Menu
//-----------------------------------------------------------------
void create_menu() {
//   int16_t header_height = IF_CHALK_ELSE(40, 24);
  male_icon = gbitmap_create_with_resource(RESOURCE_ID_MALE);
  female_icon = gbitmap_create_with_resource(RESOURCE_ID_FEMALE);
  GRect root_frame=layer_get_frame(root_layer);
  // Create the header
  menu_header_layer = layer_create(root_frame);
  menu_header_layer = layer_create(GRect(root_frame.origin.x, root_frame.origin.y, root_frame.size.w, header_height));
  layer_set_update_proc(menu_header_layer, menu_header_draw_update);
    
  // Create the menu layer
  root_frame.size.h   -= header_height;
  root_frame.origin.y += header_height;
  
  menu_layer = menu_layer_create(root_frame);
  
  menu_layer_set_callbacks(menu_layer, NULL, (MenuLayerCallbacks){
    .get_num_rows = menu_get_num_rows_callback,
    .get_header_height = menu_get_header_height_callback,
    .draw_row = menu_draw_row_callback,
    .get_cell_height = menu_get_cell_height_callback
  });
  
  // Start at the current voice in the menu
  menu_layer_set_selected_index(menu_layer, (MenuIndex){.section=0, .row=voice_choice}, MenuRowAlignCenter, false);
  window_set_click_config_provider(main_window, menu_ccp);
  layer_remove_from_parent(main_layer);
  //header_color = IF_BW_COLOR(GColorBlack, GColorVividCerulean);
  layer_add_child(root_layer, menu_header_layer);
  layer_add_child(root_layer, menu_layer_get_layer(menu_layer));
}
// ===============================================================================================================================================
// END Voice Selection Menu
// ===============================================================================================================================================


// ===============================================================================================================================================
//  Main Pebble Functions
// ===============================================================================================================================================
void main_window_load(Window *window) {
  // Set up window
  window_set_click_config_provider(window, click_config_provider);
  root_layer = window_get_root_layer(window);
  GRect root_frame=layer_get_frame(root_layer);
  
  // Add Header Layer
  header_color = IF_BW_COLOR(GColorBlack, GColorVividCerulean);
  header_layer = layer_create(GRect(root_frame.origin.x, root_frame.origin.y, root_frame.size.w, header_height));
  layer_set_update_proc(header_layer, header_draw_update);
  layer_add_child(root_layer, header_layer);
  
  // Add Main Layer
  root_frame.size.h   -= header_height;
  root_frame.origin.y += header_height;
  main_layer = layer_create(root_frame);
  layer_set_update_proc(main_layer, main_layer_update);
  layer_add_child(root_layer, main_layer);
}

void main_window_unload(Window *window) {
  //if(s_simple_menu_layer) simple_menu_layer_destroy(s_simple_menu_layer);
}

void battery_handler(BatteryChargeState charge_state) {
  static int previous_state = 0;  // To stop the friggin CONSTANT messages with no state change!
  watch_battery_level = charge_state.charge_percent;
  watch_charging = charge_state.is_charging;
  if(main_window) if(window_get_root_layer(main_window)) layer_mark_dirty(window_get_root_layer(main_window));
    
  if (charge_state.is_charging || charge_state.is_plugged) {
    if(previous_state != 1) {
      LOG("External Power Detected: Backlight On");
      previous_state = 1;
      light_enable(true);
    }
  } else {
    if(previous_state != 2) {
      LOG("Battery Power Detected: Backlight Auto");
      if(previous_state!=0)  // Stop turning backlight off at program start
        light_enable(false);
      previous_state = 2;
    }
  }
  dirty();
  LOG("New WatchBatt: %d %c", watch_battery_level, watch_charging?'Y':'N');
}

void bluetooth_handler(bool connected) {
  if(connected) {
    phone_connected = true;
  } else {
    phone_connected = false;
  }
  dirty();
}

void handle_minute_tick(struct tm *tick_time, TimeUnits units_changed) {
  char *time_format;
  if (clock_is_24h_style()) {
    time_format = "%H:%M";
  } else {
    time_format = "%I:%M%p";
  }
  strftime(time_text, sizeof(time_text), time_format, tick_time);

  if (!clock_is_24h_style()) {
    time_text[sizeof(time_text) - 2] = 0;  // Remove "M" from AM and PM
    time_text[sizeof(time_text) - 3] += 32;  // Comvert uppercase "A" or "P" to lowercase
    if(time_text[0] == '0') {
      memmove(time_text, &time_text[1], sizeof(time_text) - 1);  // Handle lack of non-padded hour format string for twelve hour clock.
    }
  }
  dirty();
}

void init() {
  //-----------------------------------------------------------------
  // Set up Pebble-Phone Communication
  //-----------------------------------------------------------------
  // Register callbacks
  app_message_register_inbox_received(inbox_received_callback);
  app_message_register_inbox_dropped(inbox_dropped_callback);
  app_message_register_outbox_failed(outbox_failed_callback);
  app_message_register_outbox_sent(outbox_sent_callback);

  app_message_open(256, 256);  // Open AppMessage with sensible buffer sizes
  
  //-----------------------------------------------------------------
  // Create main Window
  //-----------------------------------------------------------------
  main_window = window_create();
  window_set_window_handlers(main_window, (WindowHandlers) {
    .load = main_window_load,
    .unload = main_window_unload
  });
  //window_set_background_color(main_window, GColorWhite);//IF_BW_COLOR(GColorWhite, GColorVividCerulean));
  window_set_background_color(main_window, GColorWhite);//IF_BW_COLOR(GColorWhite, GColorRed));
  window_stack_push(main_window, true);
  
  //-----------------------------------------------------------------
  // Create new dictation session
  //-----------------------------------------------------------------
  IF_NOT_APLITE(dictation_session = dictation_session_create(sizeof(dictation_text), dictation_session_callback, NULL));

  //-----------------------------------------------------------------
  // Detect if running in an emulator or real watch
  //-----------------------------------------------------------------
  emulator = watch_info_get_model()==WATCH_INFO_MODEL_UNKNOWN;
  if(emulator) {
    light_enable(true);  // Good colors on emulator
    LOG("Emulator Detected: Turning Backlight On");
  } else {
    // Not an emulator, but let battery service control backlight
    battery_state_service_subscribe(battery_handler);
    battery_handler(battery_state_service_peek());
  }
  
  //-----------------------------------------------------------------
  // Subscribe to Bluetooth status and check current status
  //-----------------------------------------------------------------
  bluetooth_connection_service_subscribe(bluetooth_handler);
  bluetooth_handler(bluetooth_connection_service_peek());
  
  tick_timer_service_subscribe(MINUTE_UNIT, handle_minute_tick);
  // Prevent starting blank
  time_t now = time(NULL);
  struct tm *t = localtime(&now);
  handle_minute_tick(t, MINUTE_UNIT);

}

void deinit() {
  IF_SDK3(dictation_session_destroy(dictation_session));
  window_destroy(main_window);  // Destroy main Window
  battery_state_service_unsubscribe();
  bluetooth_connection_service_unsubscribe();
  tick_timer_service_unsubscribe();
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
