
/*******************************************************************************

 Bare Conductive Multi Board Proximity Random Touch MP3 player - Secondary Board
 ---------------------------------------
 
 secondary_board.ino - Proximity touch triggered MP3 playback, selected randomly from SD on multipule boards
 
 Based on code by Jim Lindblom and plenty of inspiration from the Freescale 
 Semiconductor datasheets and application notes.
 
 Bare Conductive code written by Stefan Dzisiewski-Smith and Peter Krige.

 Combined Proximity & Random Touch code by David Cool.
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

const int triggerPin = A0;
boolean thisTriggerValue = false;
boolean lastTriggerValue = false;

// define LED_BUILTIN for older versions of Arduino
#ifndef LED_BUILTIN
#define LED_BUILTIN 13
#endif

void setup(){  
  Wire.begin();
  MPR121.begin(MPR121_ADDR);
  MPR121.setInterruptPin(MPR121_INT);

  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);

  pinMode(triggerPin, INPUT);
  digitalWrite(triggerPin, LOW); // ensure internal pullup is disabled
 
  for(int i=0; i<12; i++){
    MPR121.setTouchThreshold(i, 40);
    MPR121.setTouchThreshold(i, 20);
  }
}

void loop(){
  processInputs();
  thisTriggerValue = digitalRead(triggerPin);
  if(thisTriggerValue && !lastTriggerValue){ // rising edge triggered
    sendSerialStatus();
  }
  lastTriggerValue = thisTriggerValue;
}

void processInputs() {
  if(MPR121.touchStatusChanged()){    
    MPR121.updateTouchData();
  }
}

void sendSerialStatus(){
  Serial1.begin(9600);
  Serial1.write('T');
    for(int i=0; i<12; i++){
      if(MPR121.getTouchData(i)){
        Serial1.write('1');
      } else {
        Serial1.write('0');
      }
    }
  Serial1.end();
}
