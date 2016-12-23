
/*******************************************************************************

 Bare Conductive Multi Board Proximity Random Touch MP3 player - Primary Board
 ---------------------------------------
 
 primary_board.ino - Proximity touch triggered MP3 playback, selected randomly from SD on multiple boards
 
 Based on code by Jim Lindblom and plenty of inspiration from the Freescale 
 Semiconductor datasheets and application notes.
 
 Bare Conductive code written by Stefan Dzisiewski-Smith and Peter Krige.

 Combined Proximity & Random Touch & Multi Board code by David Cool.
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

// mp3 includes
#include <SPI.h>
#include <SdFat.h>
#include <SdFatUtil.h> 
#include <SFEMP3Shield.h>

// number of boards config
// you can reduce this to improve response time, but the code will work fine with it 
// left at 6 - do not try to increase this beyond 6!
const int numSecondaryBoards = 6;
const int totalNumElectrodes = (numSecondaryBoards+1)*12;

// mp3 variables
SFEMP3Shield MP3player;
byte result;
int lastPlayed = 0;

// mp3 behaviour defines
#define REPLAY_MODE TRUE  // By default, touching an electrode repeatedly will 
                          // play the track again from the start each time.
                          //
                          // If you set this to FALSE, repeatedly touching an 
                          // electrode will stop the track if it is already 
                          // playing, or play it from the start if it is not.

// touch behaviour definitions
#define firstPin 0
const int lastPin = totalNumElectrodes - 1;

// sd card instantiation
SdFat sd;
SdFile file;

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

// define LED_BUILTIN for older versions of Arduino
#ifndef LED_BUILTIN
#define LED_BUILTIN 13
#endif

void setup(){  
  Serial.begin(57600);
  
  pinMode(LED_BUILTIN, OUTPUT);
   
  //while (!Serial) ; {} //uncomment when using the serial monitor 
  Serial.println("Bare Conductive Multi Board Random Touch MP3 player");

  // initialise the Arduino pseudo-random number generator with 
  // a bit of noise for extra randomness - this is good general practice
  randomSeed(analogRead(0));

  if(!sd.begin(SD_SEL, SPI_HALF_SPEED)) sd.initErrorHalt();

  if(!MPR121.begin(MPR121_ADDR)) Serial.println("error setting up MPR121");
  MPR121.setInterruptPin(MPR121_INT);

  // Changes from Touch MP3
  
  // this is the touch threshold - setting it low makes it more like a proximity trigger
  // default value is 40 for touch
  MPR121.setTouchThreshold(4);
  
  // this is the release threshold - must ALWAYS be smaller than the touch threshold
  // default value is 20 for touch
  MPR121.setReleaseThreshold(2);  

  result = MP3player.begin();
  MP3player.setVolume(10,10);
 
  if(result != 0) {
    Serial.print("Error code: ");
    Serial.print(result);
    Serial.println(" when trying to start MP3 player");
   }

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
  // only make an action if we have one or fewer pins touched
  // ignore multiple touches
  
  if(numTouches <= 1){
    for (int i=0; i < totalNumElectrodes; i++){  // Check which electrodes were pressed
      if(isNewTouch[i]){   
        //pin i was just touched
        Serial.print("pin ");
        Serial.print(i);
        Serial.println(" was just touched");
        digitalWrite(LED_BUILTIN, HIGH);

        if(i<=lastPin && i>=firstPin){
              if(MP3player.isPlaying()){
                if(lastPlayed==i && !REPLAY_MODE){
                  // if we're already playing the requested track, stop it
                  // (but only if we're in REPLAY_MODE)
                  MP3player.stopTrack();
                  Serial.print("stopping track ");
                  Serial.println(i-firstPin);
                } else {
                  // if we're already playing a different track (or we're in
                  // REPLAY_MODE), stop and play the newly requested one
                  MP3player.stopTrack();
                  playRandomTrack(i-firstPin);
                  Serial.print("playing track ");
                  Serial.println(i-firstPin);
                  
                  // don't forget to update lastPlayed - without it we don't
                  // have a history
                  lastPlayed = i;
                }
              } else {
                // if we're playing nothing, play the requested track 
                // and update lastplayed
                playRandomTrack(i-firstPin);
                Serial.print("playing track ");
                Serial.println(i-firstPin);
                lastPlayed = i;
              }
            }    
      }else{
        if(isNewRelease[i]){
          Serial.print("pin ");
          Serial.print(i);
          Serial.println(" is no longer being touched");
          digitalWrite(LED_BUILTIN, LOW);
       } 
      }
    }
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

void playRandomTrack(int electrode){

	// build our directory name from the electrode
	char thisFilename[14]; // allow for 8 + 1 + 3 + 1 (8.3 with terminating null char)
	// start with "E00" as a placeholder
	char thisDirname[] = "E00";

	if(electrode<10){
		// if <10, replace first digit...
		thisDirname[1] = electrode + '0';
		// ...and add a null terminating character
		thisDirname[2] = 0;
	} else {
		// otherwise replace both digits and use the null
		// implicitly created in the original declaration
		thisDirname[1] = (electrode/10) + '0';
		thisDirname[2] = (electrode%10) + '0';
	}

	sd.chdir(); // set working directory back to root (in case we were anywhere else)
	if(!sd.chdir(thisDirname)){ // select our directory
		Serial.println("error selecting directory"); // error message if reqd.
	}

	size_t filenameLen;
	char* matchPtr;
	unsigned int numMP3files = 0;

	// we're going to look for and count
	// the MP3 files in our target directory
  while (file.openNext(sd.vwd(), O_READ)) {
    file.getFilename(thisFilename);
    file.close();

    // sorry to do this all without the String object, but it was
    // causing strange lockup issues
    filenameLen = strlen(thisFilename);
    matchPtr = strstr(thisFilename, ".MP3");
    // basically, if the filename ends in .MP3, we increment our MP3 count
    if(matchPtr-thisFilename==filenameLen-4) numMP3files++;
  }

  // generate a random number, representing the file we will play
  unsigned int chosenFile = random(numMP3files);

  // loop through files again - it's repetitive, but saves
  // the RAM we would need to save all the filenames for subsequent access
	unsigned int fileCtr = 0;

 	sd.chdir(); // set working directory back to root (to reset the file crawler below)
	if(!sd.chdir(thisDirname)){ // select our directory (again)
		Serial.println("error selecting directory"); // error message if reqd.
	} 

  while (file.openNext(sd.vwd(), O_READ)) {
    file.getFilename(thisFilename);
    file.close();

    filenameLen = strlen(thisFilename);
    matchPtr = strstr(thisFilename, ".MP3");
    // this time, if we find an MP3 file...
    if(matchPtr-thisFilename==filenameLen-4){
    	// ...we check if it's the one we want, and if so play it...
    	if(fileCtr==chosenFile){
    		// this only works because we're in the correct directory
    		// (via sd.chdir() and only because sd is shared with the MP3 player)
				Serial.print("playing track ");
				Serial.println(thisFilename); // should update this for long file names
				MP3player.playMP3(thisFilename);
				return;
    	} else {
    			// ...otherwise we increment our counter
    		fileCtr++;
    	}
    }
  }  
}
