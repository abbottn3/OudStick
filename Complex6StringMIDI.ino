#include <MIDI.h>
#include <math.h>

#define Pot0        A9
#define Pot1        A20
#define Pot2        A21
#define Pot3        A22
#define Pot4        A0
#define Pot5        A1

#define Pick0        A3
#define Pick1        A2
#define Pick2        A5
#define Pick3        A6
#define Pick4        A7
#define Pick5        A8

#define THRESH    35     // threshold pickup value
#define N_STR     6       // number of strings
#define N_FRET    20      // number of frets CURRENTLY NOT USING
#define PICK_LOW  15     // lower threshold pickup value
#define POT_PAD   3       // minimum difference in value to classify as "changed" CURRENTLY NOT USING

MIDI_CREATE_INSTANCE(HardwareSerial, Serial1, MIDI);
const int channel = 1;
/*
#define PSTRIP A2
int pstripin = 0;
int pstripinold = 0;
int note = 0x1E;
int ledPin = 13;
*/
short Pick_vals[N_STR];
short Pick_old[N_STR];
bool Pick_active[] = {false, false, false, false, false, false}; //is it currently active
short Pick_hit[N_STR];                             //has it been hit on this loop
int Pick_pins[] = {Pick0, Pick1, Pick2, Pick3, Pick4, Pick5};
short Pick_count[N_STR];
short Pick_high[N_STR];

short Pot_vals[N_STR];                            //current sensor values
short Pot_old[N_STR];                             //old sensor values for comparison
int Pot_active[N_STR];                            //currently active notes
int Pot_pins[] = {Pot0, Pot1, Pot2, Pot3, Pot4, Pot5};
int Pot_open[N_STR];

unsigned int fretTouched[N_STR];
int Pick_offsets[N_STR];
int Thresh[N_STR] = {35, 35, 35, 60, 60, 60};

int offsets[] = {48, 53, 57, 62, 67, 72};
float c1 =  0.000004139048; 
float c2 =  -0.01243905;
float c3 =  8.382151;
float polyFit = 0;
int noteRange = 0;

int count;
int total;
const int PB_Center = 8192;
float PB_vals[N_STR];


void setup() {
  MIDI.begin();
  for(int i=0; i<N_STR; i++){
    Pick_vals[i] = 0;
    Pick_count[i] = 0;
    Pick_old[i] = 0;
    Pick_high[i] = 0;
    pinMode(Pick_pins[i], INPUT);  // set pins for pluck(string) pickups
    pinMode(Pot_pins[i], INPUT);  // set pins for note(fret) pickups
    calibration(i);
  }
  Serial.println("Calibrated");
  
}

void loop() {
  /*
  pstripin = analogRead(PSTRIP);
  note = 15 + ((pstripin/20)%60);
  if (abs(pstripin - pstripinold) > 10) {
    MIDI.sendNoteOn(note, 100, channel);
    delay(200);
    pstripinold = pstripin;
  }
  */
  readControls();
  determineFrets ();
  
  //legatoTest();
  pickNotes();
  cleanUp();
  delay(5);
  
}

void calibration(int i){
  int offset_avg = 0;
  int readerr = 0;
  for (int j = 0; j < 500; j++) {
    offset_avg += analogRead(Pick_pins[i]);
    readerr += analogRead(Pot_pins[i]);
    
    delay(5);
  }
  Pick_offsets[i] = (offset_avg / 500);

  // Temp pot calibration
  Pot_open[i] = readerr / 500;
}

// 1st
// Check all strings for pickup value (checkTriggered) and for pot value
void readControls(){ 
  //read the strings and the triggers
  for (int i=0; i<N_STR; i++){
    Pick_hit[i] = checkTriggered(i);
    Pot_vals[i] = analogRead(Pot_pins[i]);
    if (abs(Pot_vals[i] - Pot_open[i]) < 20) {
      Pot_vals[i] = 1000;
    }
    
  }
}

// This checks if the string has been plucked greater than a certain threshhold (THRESH).
// If it has, then return the value. If it hasn't, then check if it's low enough to be
// be considered "off". If it is, set it to inactive.
short checkTriggered(int i){
  short v = abs(analogRead(Pick_pins[i]) - Pick_offsets[i]);
  short ret = 0;
  /*
  if (v > Pick_old[i]) {
    Pick_old[i] = v;
  }
  */
  if(!Pick_active[i] && v > Thresh[i]){   // if it wasn't already reading and it's greater than threshold
    Pick_count[i] += 1;
    if (v > Pick_high[i]) {
      Pick_high[i] = v;
    }
    if (Pick_count[i] > 2) {
      Pick_active[i] = true;
      Pick_vals[i] = Pick_high[i];
      ret = Pick_high[i];
      Pick_high[i] = 0;
    }
  }
  
  if(Pick_active[i] && v < PICK_LOW){  // if it WAS active, but is now low enough to be considered off, turn off
    Pick_count[i] += 1;
    if (Pick_count[i] > 35) {
      Pick_active[i] = false;
      Pick_vals[i] = 0;
      Pick_count[i] = 0;
    }
  }
  if (Pick_count[i] > 1000) {
    Pick_count[i] = 0;
  }
  return ret;
}

// 2nd
// This takes all of the potentiometer values and determines fretTouched[i] values
// This basically converts Pot_vals to fretTouched. Pot_val is used to determine
// if a new fret has been touched.
void determineFrets () {    
  
   //---------Get Fret Numbers------
 for (int i=0; i< N_STR; i++) {
 
   short pot_val = Pot_vals[i];
   polyFit = (c1*pow(pot_val,2)) + (c2*pot_val) + c3;
   fretTouched[i] = round(polyFit);
   double fractpart;
   short bend_diff = modf (polyFit, &fractpart);
   PB_vals[i] = PB_Center + (bend_diff * PB_Center/2);
   MIDI.sendPitchBend(PB_vals[i], i);
    
  }
  
  
}
// 3rd
// Channels?
// This is just saying if the note changes without a strum, send that note.
// Might be able to use MIDI.sendPitchBend
void legatoTest(){    
  for(int i=0; i<N_STR; i++){
    if(Pot_active[i]){
      int note = fretTouched[i] + offsets[i];
      if(note != Pot_active[i] && (fretTouched[i] || Pick_active[i])){   // if the note changes (fret or strum), send that data

        MIDI.sendPitchBend(PB_vals[i], i);
        
        Pot_active[i] = note;
      }
    }
  }
}

// 4th
// If a strum is detected, turn off the last note on that string and send a new one
void pickNotes(){       
  for (int i=0; i<N_STR; i++){
    if(Pick_hit[i]){
      if(Pot_active[i]){
        //turn off active note on this string
        MIDI.sendNoteOff(Pot_active[i], 0, i);
        //noteOff(0x80 + channel, Pot_active[i]);
      }
      Pot_active[i] = fretTouched[i] + offsets[i];
      MIDI.sendNoteOn(Pot_active[i], 100, i);
      
      Serial.print(Pot_active[i]);
      Serial.print(", ");
      Serial.print(Pick_hit[i]);
      Serial.print(", ");
      Serial.println(i);
      
      
      //noteOn(0x90 + channel, Pot_active[i], 100);
    }
  }
}
// 5th
// 
void cleanUp(){ 
  for (int i=0; i<N_STR; i++){
    if(Pot_active[i] && !fretTouched[i] && !Pick_active[i]){
        if (Pot_active[i] != offsets[i]) {
          MIDI.sendNoteOff(Pot_active[i], 0, i);
          Pot_active[i] = 0;
        }       
    }
  }
}

/* Notes:
 * - fretTouched and Pot_active hold note values
 * - confused about the channel
 * - 8192 is pitchbend offset
 */

