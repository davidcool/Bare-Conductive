/*******************************************************************************

 Bare Conductive Multi Board Midi Interface
 ------------------------------------------
 
 primary_board.ino - touch triggered MIDI from multiple Touch Boards

 Touch electrodes 0 to 11 locally to play notes 0 - 11 from 
 the primary board. Touch electrodes 0 to 11 on the first secondary board
 to play notes 12 - 23 from the primary board. Touch 
 electrodes 0 to 11 on the second secondary board to play notes 24 - 35 from the primary board, and so on. 

 Maximum total number of boards is 7 (one primary board and six 
 secondary boards). All boards must share a common ground connection,
 with TX on all the secondary boards commoned together and connected to RX on
 the primary board. The first secondary board must have a connection between 
 its A0 and A0 on the primary board. The second secondary board must have a 
 connection between its A0 and A1 on the primary board, and so on.

 Each board must also be powered, although up to 4 boards can share power by
 commoning up the 5V connection between them.
 
 Based on code by Jim Lindblom and plenty of inspiration from the Freescale 
 Semiconductor datasheets and application notes.
 
 Bare Conductive code written by Stefan Dzisiewski-Smith and Peter Krige.

 Combined MIDI & Multi Board code by David Cool.
 http://davidcool.com
 
 This work is licensed under a Creative Commons Attribution-ShareAlike 3.0 
 Unported License (CC BY-SA 3.0) http://creativecommons.org/licenses/by-sa/3.0/
 
 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 THE SOFTWARE.

*******************************************************************************/

// compiler error handling
#include "Compiler_Errors.h"

// touch includes
#include <MPR121.h>
#include <Wire.h>
#define MPR121_ADDR 0x5C
#define MPR121_INT 4

MIDIEvent e;

// number of boards config
// you must set this to exactly the number of boards connected or your note range will be off!!!
// do not try to increase this beyond 6!
const int numSecondaryBoards = 1;
const int totalNumElectrodes = (numSecondaryBoards+1)*12;

int lastPlayed = 0;

// serial comms definitions
const int serialPacketSize = 13;

// secondary board touch variables
bool thisExternalTouchStatus[numSecondaryBoards][12];
bool lastExternalTouchStatus[numSecondaryBoards][12];

// compound touch variables
bool touchStatusChanged = false;
bool isNewTouch[totalNumElectrodes];
bool isNewRelease[totalNumElectrodes];
int numTouches = 0;

void setup(){  
  Serial.begin(57600);

  e.type = 0x08;
  e.m3 = 127;  // maximum volume
  
  pinMode(LED_BUILTIN, OUTPUT);
   
  //while (!Serial) {}; //uncomment when using the serial monitor 
  Serial.println("Bare Conductive Multi Board MIDI Interface");

  if(!MPR121.begin(MPR121_ADDR)) Serial.println("error setting up MPR121");
  MPR121.setInterruptPin(MPR121_INT);

  for(int i=0; i<numSecondaryBoards; i++){
    for(int j=0; j<12; j++){
      thisExternalTouchStatus[i][j] = false;
      lastExternalTouchStatus[i][j] = false;
    }
  }

  for(int i=0; i<totalNumElectrodes; i++){
    isNewTouch[i] = false;
    isNewRelease[i] = false;
  }   

  for(int a=A0; a<=A5; a++){
    pinMode(a, OUTPUT);
    digitalWrite(a, LOW); 
  }

  Serial1.begin(9600);
  delay(100);
}

void loop(){
  
  // reset everything  that we combine from the two boards
  resetCompoundVariables();
  
  readLocalTouchInputs();
  
  readRemoteTouchInputs();
  
  processTouchInputs();
  
}


void readLocalTouchInputs(){

  // update our compound data on the local touch status

  if(MPR121.touchStatusChanged()){
    MPR121.updateTouchData();
    touchStatusChanged = true;
    
    for(int i=0; i<12; i++){
      isNewTouch[i] = MPR121.isNewTouch(i);
      isNewRelease[i] = MPR121.isNewRelease(i);
    }
  }
  numTouches+=MPR121.getNumTouches();

}

void readRemoteTouchInputs(){

  char incoming;

  for(int a=A0; a<A0+numSecondaryBoards; a++){

    digitalWrite(a, HIGH);
    delay(15);

    // only process if we have a full packet available
    while(Serial1.available() >= serialPacketSize){

      // save last status to detect touch / release edges
      for(int i=0; i<12; i++){
        lastExternalTouchStatus[a-A0][i] = thisExternalTouchStatus[a-A0][i];
      }
      
      incoming = Serial1.read();
      if(incoming == 'T'){ // ensure we are synced with the packet 'header'
        for(int i=0; i<12; i++){
          if(!Serial1.available()){
            return; // shouldn't get here, but covers us if we run out of data
          } else {
            if(Serial1.read()=='1'){
              thisExternalTouchStatus[a-A0][i] = true;
            } else {
              thisExternalTouchStatus[a-A0][i] = false;
            }
          }
        }
      } 
    }

    // now that we have read the remote touch data, merge it with the local data
    for(int i=0; i<12; i++){
      if(lastExternalTouchStatus[a-A0][i] != thisExternalTouchStatus[a-A0][i]){
        touchStatusChanged = true;
        if(thisExternalTouchStatus[a-A0][i]){
          // shift remote data up the array by 12 so as not to overwrite local data
          isNewTouch[i+(12*((a-A0)+1))] = true;
        } else {
          isNewRelease[i+(12*((a-A0)+1))] = true;
        }
      }

      // add any new touches to the touch count
      if(thisExternalTouchStatus[a-A0][i]){
        numTouches++;
      }
    }

    digitalWrite(a, LOW);
  }
}

void processTouchInputs(){
  // limit number of touches to amount of available electrodes
  if(numTouches <= totalNumElectrodes){
    for (int i=0; i < totalNumElectrodes; i++){  // Check which electrodes were pressed

      // MIDI note mapping from electrode number to MIDI note
      e.m2 = 48 + totalNumElectrodes - 1 - i;
      
      if(isNewTouch[i]){   
        //pin i was just touched
        Serial.print("pin ");
        Serial.print(i);
        Serial.println(" was just touched");
        digitalWrite(LED_BUILTIN, HIGH);
        e.m1 = 0x90;    
      } else if(isNewRelease[i]){
          Serial.print("pin ");
          Serial.print(i);
          Serial.println(" is no longer being touched");
          digitalWrite(LED_BUILTIN, LOW);
          e.m1 = 0x80;
       } else {
          // else set a flag to do nothing...
          e.m1 = 0x00;
        }
        // only send a USB MIDI message if we need to
        if(e.m1 != 0x00){
        MIDIUSB.write(e);
    }
  }
  // flush USB buffer to ensure all notes are sent
  MIDIUSB.flush();
}
}


void resetCompoundVariables(){

  // simple reset for all coumpound variables

  touchStatusChanged = false;
  numTouches = 0;

  for(int i=0; i<totalNumElectrodes; i++){
    isNewTouch[i] = false;
    isNewRelease[i] = false;
  }  
}
