
/* Disclaimer: this document is an amaglamation of several different documents I found scattered throughout the internet. I will do my best to document where I found and how I implemented it, but in all honesty I wrote this over a period of 6 months, and haven't looked at it for 3 months so there's some I'm absolutely clueless about. 

Sources:
  Adalight-FastLED, Adalight - https://github.com/dmadison/Adalight-FastLED
    -> this runs the responsive light mode
  
Libraries:
  - FastLED: this is my favorite arduino LED driver library
  - IRremote: ligrary that governs the IR sensor and remote


There are several different light modes I have implemented here
1. Solid Color Modes -> these are straight from FASTLED, super simple just fills all the leds with the same color
2. Breath Mode -> takes the solid colors and cycles the brightness up and down
3. Reactive Color Mode -> responsive color based on the colors on a connected computer screen, this connects to the external application Prismatik    *Disclaimer: I've only ever gotten Prismatik to work on a Windows computer, they have a Mac version but I wasn't able to get it to work 
4. Rainbow Mode -> cycles smoothly through the rainbow
*/


// libraries for IR remote and Arduino
#include <Arduino.h> 
#include <IRremote.hpp>

#define IR_RECEIVE_PIN GPIO_NUM_15
#define DECODE_NEC

// these are definitions handled by the ESP32 Remote (RMT) functions
#define FASTLED_RMT_MAX_CHANNELS 2
//#define FASTLED_RMT_BUILTIN_DRIVER 1
#define FASTLED_ESP32_FLASH_LOCK 1

uint8_t max_bright = 255; 
uint8_t last_bright = 0; //not sure if this is necessary but return to it

#define LED_PIN1 GPIO_NUM_13  // Data Pin for the first Strip
#define LED_PIN2 GPIO_NUM_14  // Data Pin for the mirrored strip

#define COLOR_ORDER RGB      // It's GRB for WS2812 and BGR for APA102.
#define LED_TYPE WS2812B     // Using APA102, WS2812, WS2801. Don't forget to modify LEDS.addLeds to suit.
#define NUM_LEDS 724         // Number of LED's.

#define NUM_LEDS_PER_STRIP 400         // Number of LEDs per strip *still necessary if not mirroring*



const unsigned long
  SerialSpeed = 115200;  // serial port speed


// Adalight Serial Definitions
const uint16_t
  SerialTimeout = 0;  // time before LEDs (in Adalight mode) are shut off if no data (in seconds), 0 to disable

#define SERIAL_FLUSH  //not 100% sure what this does




/*=================================================================*/
#include <FastLED.h>

//MY LED CODE DEFINITIONS
bool ON = true;

uint8_t r = 255; //RED
uint8_t g = 0; //GREEN
uint8_t b = 0; //BLUE
uint8_t sr = 0; //solid red
uint8_t sg = 0; //solid green
uint8_t sb = 0; //solid blue
uint8_t colorStep = 0; //init colorstep for rainbow()

int holder_code = 5;  //reactive mode on start
int prev_holder_code = 1; //this might be defunct 
int older_holder_code = 8; //this is kinda really defunct
void handleIR(); 
/*=================================================================*/

//definitions to initialize FASTLED

struct CRGB leds[NUM_LEDS_PER_STRIP];
uint8_t *ledsRaw = (uint8_t *)leds;

/*==============================================================================================*/
//ADALIGHT DEFINITIONS 

const uint8_t magic[] = {
  'A', 'd', 'a'
};
#define MAGICSIZE sizeof(magic)

// Check values are header byte # - 1, as they are indexed from 0
#define HICHECK (MAGICSIZE)
#define LOCHECK (MAGICSIZE + 1)
#define CHECKSUM (MAGICSIZE + 2)

enum processModes_t { Header, Data} mode = Header;

int16_t c;                                   // current byte, must support -1 if no data available
uint16_t outPos;                             // current byte index in the LED array
uint32_t bytesRemaining;                     // count of bytes yet received, set by checksum
unsigned long t, lastByteTime, lastAckTime;  // millisecond timestamps

void headerMode();
void dataMode();
void timeouts();

// Macros initialized
#ifdef SERIAL_FLUSH
  #undef SERIAL_FLUSH
  #define SERIAL_FLUSH while (Serial.available() > 0) { Serial.read(); }
#else
  #define SERIAL_FLUSH
#endif


/*=================================================================================================*/


void setup() {

  FastLED.addLeds<LED_TYPE, LED_PIN1, COLOR_ORDER>(leds, NUM_LEDS_PER_STRIP);  
  FastLED.addLeds<LED_TYPE, LED_PIN1, COLOR_ORDER>(leds, NUM_LEDS_PER_STRIP);
  FastLED.addLeds<LED_TYPE, LED_PIN2, COLOR_ORDER>(leds, NUM_LEDS_PER_STRIP);
  FastLED.addLeds<LED_TYPE, LED_PIN2, COLOR_ORDER>(leds, NUM_LEDS_PER_STRIP);

  /*if not mirroring strips just use
  FastLED.addLeds<LED_TYPE, LED_PIN1, COLOR_ORDER>(leds, NUM_LEDS_PER_STRIP);
  */

  FastLED.setBrightness(max_bright);
  //FastLED.setMaxPowerInVoltsAndMilliamps(5, 500);               // FastLED Power management set at 5V, 500mA.

  IrReceiver.begin(IR_RECEIVE_PIN);

  Serial.begin(115200);   // Initialize serial port for debugging.
  Serial.print("Ada\n");  // Send ACK string to host                                               

  //Serial.println("Starting Serial Monitor");


  lastByteTime = lastAckTime = millis();  // Set initial counters

  /*    COMMENTED OUT BC CANT USE SERIAL MONITOR W ADALIGHT
  Serial.print(F("Ready to receive IR signals of protocols: "));
  printActiveIRProtocols(&Serial);
  Serial.print(F("at pin "));
  Serial.println(IR_RECEIVE_PIN);
  */
}

void loop() {
  /*this code is dependent on the holder_code variable. When the IR sensor receives and decodes a signal,
   it triggers the handleIR() funciton that has a switch case for all the commands, this case sets the 
   holder_code variable.

   holder_code glossary:
   1 = all solid color modes -  the switch case also sets the sr sg sb variables, which determine the solid fill rgb values
   2 = brightness up/down buttons
   3 = off mode
   4 = on button
   5 = reactive color mode
   6 = strobe button - currently not set to a mode -> i could make something new if I have time but I probably will not
   7 = fade button - breath mode
   8 = smooth button - rainbow mode 
   */

  t = millis();  // Save current time

//if ir receiver recieves a code from the remote
  if (IrReceiver.decode()) {

    prev_holder_code = holder_code; //sets previous holder code value, could be defunct
    if (IrReceiver.decodedIRData.address == 61184){
      //Serial.println(IrReceiver.decodedIRData.decodedRawData); -> uncomment this to see the raw hex codes displayed in serial monitor [ADALIGHT MUST BE INACTIVE FOR THIS TO WORK BECAUSE ADALIGHT UTILIZES THE SERIAL MONITOR TO COMMUNICATE WITH PRISMATIK]
      handleIR();
    }

    IrReceiver.resume();
  }


//if statements that execute color modes based on holder code values 
  if (holder_code == 1) {  //ALL SOLID COLOR MODES

    max_bright = 255;
    fill_solid(leds, NUM_LEDS_PER_STRIP, CRGB(sr, sg, sb));
    //FastLED.setBrightness(max_bright);            //OK so here I'm taking this out bc if I do it right then the ON bool will set it for me???
    FastLED.show();                                //THE ADALIGHT CODE HAS FASTLED IN THE MIDDLE OF ITS OTHER FUNCTIONS SO I'M PUTTING IT HERE
} 
  
  else if (holder_code == 2) {  //BRIGHTNESS UP/DOWN
//EMPTY BUT I NEED TO FIX IT
} 
  else if (holder_code == 3) {  //OFF
    // max_bright = 0;
    // FastLED.setBrightness(max_bright);
    // FastLED.show();
} 

  else if (holder_code == 4) {  //ON

  }


  /*=================================================================*/
  //REACTIVE COLOR MODE
  else if (holder_code == 5) {  //FLASH    
    if ((c = Serial.read()) >= 0) {
      lastByteTime = lastAckTime = t;  // Reset timeout counters

      switch (mode) {
        case Header:
          headerMode();
          break;
        case Data:
          dataMode();
          break;
      }
    }
    else {
      // No new data
      timeouts();
    }
  }
/*=================================================================*/


  else if (holder_code == 6) {  //STROBE button- going to be something else???

  }

  else if (holder_code == 7) {  //FADE button - breath mode
    last_bright = max_bright;
    max_bright = beatsin16(14, 15, 255); //FINAL the last int here was 160 bc the lights before had little difference between 160 and 255, changed to 255 but not tested
    // FastLED.setBrightness(max_bright);
    FastLED.show();
  } 

  else if (holder_code == 8) {  //SMOOTH
    rainbow();
    delay(10);
    FastLED.show();
  }

  else{
    delay(20);    
  }

//[THIS BLOCK IS THE BRIGHTNESS UP/DOWN BUTTONS THAT I WAS NEVER REALLY ABLE TO FIGURE OUT]
  // if(prev_holder_code == 7 && holder_code !=7 && holder_code ){
  //   max_bright = 255;
  //   FastLED.setBrightness(max_bright);
  //   FastLED.show();
  // }


  if (ON == true){
    FastLED.setBrightness(max_bright);
    //FastLED.show(); 
  }else if(ON == false){
    FastLED.setBrightness(0);
    FastLED.show(); 

  }
}


void handleIR() {
  delay(50);
  
  //THIS IS PRETTY DEFUNCT I BELIEVE
  // if (older_holder_code != prev_holder_code && prev_holder_code != 3){
  //   older_holder_code = prev_holder_code;
  //   if (prev_holder_code != holder_code){
  //       prev_holder_code = holder_code;
  //   }
  // }




  switch (IrReceiver.decodedIRData.command) {
    //0  BRIGHTNESS UP
    case 0:
      //holder_code = 2;
      // if (max_bright <= 255 && max_bright >= 0) {
      //   max_bright += 10;
      //   // FastLED.setBrightness(max_bright);
      //   // FastLED.show();
      // }
      delay(20);
      // Serial.println(max_bright);
      break;

    //1 BRIGHTNESS DOWN         *I might change this to bpm if I want to make that disco mode*
    case 1:
      // Serial.println(max_bright);
      //holder_code = 2;
      // if (max_bright <= 255 && max_bright >= 0) {
      //   max_bright -= 10;
      // }
      delay(20);
      break;


    //2 - OFF
    case 2:
      ON = false;
      delay(20);
      break;

    //3 - ON
    case 3:
      ON = true;
      delay(20);
      break;

    //4 - RED
    case 4:
      // Serial.println("     RED");
      holder_code = 1;
      sr = 255;
      sg = 0;
      sb = 0;
      delay(20);
      break;

    // 8 IS ORANGE
    case 8:
      // Serial.println("     ORANGE");
      holder_code = 1;
      sr = 255;
      sg = 55;
      sb = 0;
      delay(20);
      break;

    //12 iS LIGHT ORANGE
    case 12:
      // Serial.println("     LIGHT ORANGE");
      holder_code = 1;
      sr = 255;
      sg = 85;
      sb = 0;
      delay(20);
      break;

    //16 IS YELLOW/ORANGE
    case 16:
      // Serial.println("     YELOW/ORANGE");
      holder_code = 1;
      sr = 255;
      sg = 130;
      sb = 0;
      delay(20);
      break;

    //20 IS YELLOW
    case 20:
      // Serial.println("     YELlOW");
      holder_code = 1;
      sr = 255;
      sg = 200;
      sb = 0;
      delay(20);
      break;

    //5 IS GREEN
    case 5:
      // Serial.println("     GREEN");
      holder_code = 1;
      sr = 0;
      sg = 255;
      sb = 0;
      delay(20);
      break;

    //9 IS LIGHT GREEN
    case 9:
      // Serial.println("     LIGHT GREEN");
      holder_code = 1;
      sr = 50;
      sg = 255;
      sb = 30;
      delay(20);
      break;

    //13 IS LIGHT TEAL
    case 13:
      // Serial.println("     LIGHT TEAL");
      holder_code = 1;
      sr = 70;
      sg = 200;
      sb = 120;
      delay(20);
      break;

    //17 IS MED TEAL
    case 17:
      // Serial.println("     MEDIUM TEAL");
      holder_code = 1;
      sr = 5;
      sg = 255;
      sb = 90;
      delay(20);
      break;

    //21 IS DARK TEAL
    case 21:
      // Serial.println("     DARK TEAL");
      holder_code = 1;
      sr = 20;
      sg = 255;
      sb = 50;
      delay(20);
      break;

    //6 IS DARK BLUE
    case 6:
      // Serial.println("     DARK BLUE");
      holder_code = 1;
      sr = 0;
      sg = 0;
      sb = 255;
      delay(20);
      break;

    //10 IS MED BLUE
    case 10:
      // Serial.println("     MEDIUM BLUE");
      holder_code = 1;
      sr = 50;
      sg = 70;
      sb = 255;
      delay(20);
      break;

    //14 IS PURPLE
    case 14:
      // Serial.println("     PURPLE");
      holder_code = 1;
      sr = 230;
      sg = 0;
      sb = 255;
      delay(20);
      break;

    //18 IS MAGENTA
    case 18:
      // Serial.println("     MAGENTA");
      holder_code = 1;
      sr = 255;
      sg = 0;
      sb = 150;
      delay(20);
      break;

    //22 IS PINK
    case 22:
      // Serial.println("     PINK");
      holder_code = 1;
      sr = 255;
      sg = 0;
      sb = 50;
      delay(20);
      break;

    //7 IS WHITE
    case 7:
      // Serial.println("     WHITE");
      holder_code = 1;
      sr = 255;
      sg = 255;
      sb = 255;
      delay(20);
      break;

    //11 IS FLASH
    case 11:
      // Serial.println("     FLASH");
      max_bright = 255;
      holder_code = 5;
      delay(20);
      break;

    //15 IS STROBE
    case 15:
      // Serial.println("     STROBE");
      holder_code = 6;
      delay(20);
      break;

    //19 IS FADE
    case 19:
      // Serial.println("     FADE");
      holder_code = 7;
      delay(20);
      break;

    //23 IS SMOOTH
    case 23:
      // Serial.println("     SMOOTH");
      max_bright = 255;
      holder_code = 8;
      delay(20);
      break;
  }

  delay(30);
}

// =================================================================================================================================
//THESE ARE THE ADALIGHT FUNCTIONS
void headerMode() {
   if (holder_code == 5) {
    static uint8_t
      headPos,
      hi, lo, chk;

    if (headPos < MAGICSIZE) {
      // Check if magic word matches
      if (c == magic[headPos]) {
        headPos++;
      } else {
        headPos = 0;
      }
    } else {
      // Magic word matches! Now verify checksum
      switch (headPos) {
        case HICHECK:
          hi = c;
          headPos++;
          break;
        case LOCHECK:
          lo = c;
          headPos++;
          break;
        case CHECKSUM:
          chk = c;
          if (chk == (hi ^ lo ^ 0x55)) {
            // Checksum looks valid. Get 16-bit LED count, add 1
            // (# LEDs is always > 0) and multiply by 3 for R,G,B.
            // D_LED(ON);
            bytesRemaining = 3L * (256L * (long)hi + (long)lo + 1L);
            outPos = 0;
            memset(leds, 0, NUM_LEDS_PER_STRIP * sizeof(struct CRGB));
            mode = Data;  // Proceed to latch wait mode
          }
          headPos = 0;  // Reset header position regardless of checksum result
          break;
      }
    }
   }
}

void dataMode() {
  if (holder_code == 5) {
  // If LED data is not full
    if (outPos < sizeof(leds)) {
      ledsRaw[outPos++] = c;  // Issue next byte
    }
    bytesRemaining--;

    if (bytesRemaining == 0) {
      // End of data -- issue latch:
      mode = Header;  // Begin next header search
      FastLED.show();
      // D_FPS;
      // D_LED(OFF);
      SERIAL_FLUSH;
    }
  }
}

void timeouts() {
  if (holder_code == 5) {
    // No data received. If this persists, send an ACK packet
    // to host once every second to alert it to our presence.
    if ((t - lastAckTime) >= 1000) {
      Serial.print("Ada\n");  // Send ACK string to host
      lastAckTime = t;        // Reset counter

      // If no data received for an extended time, turn off all LEDs.
      if (SerialTimeout != 0 && (t - lastByteTime) >= (uint32_t)SerialTimeout * 1000) {
        memset(leds, 0, NUM_LEDS_PER_STRIP * sizeof(struct CRGB));  //filling Led array by zeroes
        FastLED.show();
        mode = Header;
        lastByteTime = t;  // Reset counter
      }
    }
  }
}
// ============================================================================================================================================



void rainbow() {
  /*This funciton cycles through the rainbow smoothly, based on the r g and b variable values it increments */

  //start from red go into purple
  //r=255 g=0 b=0 -> r=255 g=0 b=255
  if (r == 255 && g == 0 && b < 255) {
    if (colorStep < 255 && colorStep >= 0) {
      colorStep++;
      b = colorStep;
    } else if (colorStep == 255) {
      b = 255;
    }
    delay(10);
  }

  //start from purple go into blue
  //r=255 g=0 b=255 -> r=0 g=0 b=255
  if (r > 0 && g == 0 && b == 255) {
    if (colorStep <= 255 && colorStep > 0) {  //keeps colorStep from going over 255 or under 0
      colorStep--;
      r = colorStep;
    } else if (colorStep == 0) {
      r = 0;
    }
    delay(10);
  }

  //start from blue go into turquoise
  //r=0 g=0 b=255 -> r==0 g==255 b==255
  else if (r == 0 && g < 255 && b == 255) {
    if (colorStep < 255 && colorStep >= 0) {  //keeps colorStep from going over 255 or under 0
      colorStep++;
      g = colorStep;
    } else if (colorStep == 255) {
      g = 255;
    }
    delay(10);
  }

  //start from turquoise go into green
  //r==0 g==255 b==255 -> r==0 g==255 b==0
  else if (r == 0 && g == 255 && b > 0) {
    if (colorStep <= 255 && colorStep > 0) {  //keeps colorStep from going over 255 or under 0
      colorStep--;
      b = colorStep;
    } else if (colorStep == 0) {
      b = 0;
    }
    delay(10);
  }

  //start from green go into yellow
  //r==0 g==255 b==0 -> r==255 g==255 b==0
  else if (r < 255 && g == 255 && b == 0) {
    if (colorStep < 255 && colorStep >= 0) {  //keeps colorStep from going over 255 or under 0
      colorStep++;
      r = colorStep;
    } else if (colorStep == 255) {
      r = 255;
    }
    delay(10);
  }

  //start from yellow go back to red
  //r==255 g==255 b==0 ->   //r=255 g=0 b=0
  else if (r == 255 && b == 0 && g > 0) {
    if (colorStep <= 255 && colorStep > 0) {  //keeps colorStep from going over 255 or under 0
      colorStep--;
      g = colorStep;
    } else if (colorStep == 0) {
      g = 0;
    }
    delay(10);
  }

  for (int x = 0; x < NUM_LEDS_PER_STRIP; x++) {
    leds[x] = CRGB(r, g, b);
  }
}
// =====================================================================================
