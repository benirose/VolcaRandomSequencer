#include <TimerOne.h>

/***********************************************************************************
    Volca Beats Random Sequencer v1.0
      Random 16th note sequencer with probability control, velocity range control, tempo control, glitch mode, and double time trigger.
      Based on previous random note sequencer made for the EMX, modified for use with the Volca Beats.
      See https://www.youtube.com/watch?v=Clm4hhMMvuI for full explanation.

    The original project only had 3 knobs, one switch, and one button for control, so this project has those same limitations.
    Future versions may incorporate more control, such as stutter randomization and MIDI input for clock sync.

    The project is very simple and requires few components. In order to built this project you will need:
    - 3 potentiometers, 10k recommended
    - 1 SPST switch
    - 1 tactile button
    - 1 220k ohm resister for the MIDI output circuit
    - 1 MIDI jack, or a spliced MIDI cable like I used

    Connect the 3 potentiometers as follows:
    - left pin to GND
    - right pin to 5v
    - center pin to A0-A2 on Arduino

    The pins are designated as follows:
    - A0: Velocity control
    - A1: Probability control
    - A2: Tempo control

    Connect the SPST switch as follows:
    - left pin to GND
    - right pin to 5v
    - center pin to pin 3 on Arduino

    When the switch is in the right position, randomized CC parameters for hihat, tom, and PCM drum sounds will be sent when they are triggered, creating a more glitchy sound.
    You can uncomment code in the loop to also send stutter randomization when this switch is in the right position, which brings the glitchy-ness up even more.

    Connect the tactile button between GND and pin 2 on the Arduino.
    When you depress the tactile button, the sequencer will trigger 32nd notes instead of 16th notes.

    See https://www.arduino.cc/en/Tutorial/Midi for information on how to hook up the midi jack.
    I skipped the 220 ohm resistor from pin 1 on the Arduino (TX) as many tutorials do not include it.

    NOTE: You may have problems uploading the sketch while pin 1 on the Arduino (TX) is in use or connected. Simply disconnect that pin in order to upload the sketch.

 ***********************************************************************************/

#include <MIDI.h>

// MIDI notes for each voice
#define KICK 36
#define SNARE 38
#define LO_TOM 43
#define HI_TOM 50
#define C_HAT 42
#define O_HAT 46
#define CLAP 39
#define CLAVES 75
#define AGOGO 67
#define CRASH 49

// Some constants used in pin reading
#define PLAY 3
#define EIGHTH 4
#define SIXTEENTH 5
#define THIRTYSECOND 6
#define TEMPO A5
#define PROBABILITY A1
#define VELOCITY_LOW A2
#define VELOCITY_HIGH A3
#define TEMPO_LED 13


// set up an array of available notes to trigger
static const int drumNotes[] = { KICK, SNARE, LO_TOM, HI_TOM, C_HAT, O_HAT, CLAP, CLAVES, AGOGO, CRASH };

// some variables to store values in as we're sequencing
volatile int note = 0;
volatile float bpm;
volatile float tempoDelay = 125;
volatile int stepsPerQuarter = 4;
volatile int clocksPerStep = 24 / stepsPerQuarter;
volatile int probability;
volatile int velocity;
volatile int min_velocity;
volatile int max_velocity;
volatile int velocity_range;
volatile boolean midiSync = false;
volatile int stepCount = 0;
volatile boolean playing = false;
volatile long debounce = 0;
volatile long timerDelay = 0;
volatile long clockDelay = 0;



// Call this in order to set up MIDI
MIDI_CREATE_DEFAULT_INSTANCE();

void setup(void)
{
  MIDI.begin(MIDI_CHANNEL_OMNI);          // Launch MIDI and listen to channel 10
  MIDI.turnThruOff();
  randomSeed(100);         // seed our randomizer, there might be better ways to do this

  // See https://www.arduino.cc/en/Tutorial/DigitalPins to learn more about Arduino's built in pull up resistors
  pinMode(PLAY, INPUT_PULLUP);
  pinMode(EIGHTH, INPUT_PULLUP);
  pinMode(SIXTEENTH, INPUT_PULLUP);
  pinMode(THIRTYSECOND, INPUT_PULLUP);
  
  // Set up tempo led for output
  pinMode(TEMPO_LED, OUTPUT);
  debounce = millis();

  // start timer
  Timer1.initialize(1000);
  Timer1.attachInterrupt(handleTimer);
  timerDelay = millis();

  // attach play/stop interrupt
  attachInterrupt(digitalPinToInterrupt(3), handleStartStop, FALLING);

  // attach MIDI clock callback
  MIDI.setHandleClock(handleMidiClock);
  // attach MIDI start callback
  MIDI.setHandleStart(handleMidiStart);
  // attach MIDI stop callback
  MIDI.setHandleStop(handleMidiStop);
  clockDelay = millis();
}

void handleStartStop(void)
{
  if (millis() - debounce > 50)
  {
    debounce = millis();
    playing = !playing;
    stepCount = 0;
    timerDelay = millis();
    clockDelay = millis();
  }
}

void handleTimer(void)
{
  // if midi synced and still getting midi clock (one less than 500ms ago)
  if (midiSync && (millis() - clockDelay) < 500)
  {
    return;
  } else if (midiSync) {
    // we haven't gotten a midi clock in 500ms, go back to internal sync
    midiSync = false;
    stepCount = 0;
    playing = false;    
  }
  
  
  // return if enough time hasn't ellapsed since last step
  if ((millis() - timerDelay) < (tempoDelay))
  {
    return;
  }
  timerDelay = millis();
  
  if (playing)
  {
    playNote();
  }

  
  // brink tempo led once per quarter note
  if (stepCount < stepsPerQuarter / 2)
  {
    digitalWrite(TEMPO_LED, HIGH);
  } else {
    digitalWrite(TEMPO_LED, LOW);
  }

  // increase step count
  stepCount++;
  // if we've had as many steps as are in a quarter note, start over
  if (stepCount >= stepsPerQuarter)
  {
    stepCount = 0;
  }
}

void handleMidiStart(void)
{
  playing = true;
  stepCount = 0;
}
void handleMidiStop(void)
{
  playing = false;
  stepCount = 0;
}

void handleMidiClock(void)
{
  midiSync = true;
  clockDelay = millis();

  // get our clock pulses per step
  clocksPerStep = 24 / stepsPerQuarter;
  
  // if playing and we are on a sixteenth note (every 6 pulses of clock)
  if (playing && (stepCount % clocksPerStep == 0))
  {
    playNote();
  }

  if (stepCount < 12)
  {
    digitalWrite(TEMPO_LED, HIGH);
  } else {
    digitalWrite(TEMPO_LED, LOW);
  }

  // increase step count
  stepCount++;
  // if we've had as many steps as are in a quarter note, start over
  if (stepCount >= 24)
  {
    stepCount = 0;
  }
}

void playNote(void)
{
  note = random(4);    // Choose random note (drum)
  velocity = random(velocity_range);     // difference between min and max, or 0 if max is less than min
  velocity += min_velocity;      // always add random value from range to min. If range is 0, min will always be our velocity

  int prob = random(1024); // Random value for probability testing

  if (probability > 0 && prob <= probability)
  {
    // If we have the glitch switch turned on
//    if (digitalRead(FSU) == HIGH)
//    {
//      /*
//         Uncomment the below block for stutter randomization
//         I tried to use musical settings, with 20% of depth randomization
//         resulting in no stutter, and the speed only going to 80 (out of 127)
//      */
////       int depth = random(100);
////       depth = (depth < 20) ? 0 : depth-20;
////       MIDI.sendControlChange(54, random(80), 10);
////       MIDI.sendControlChange(55, depth, 10);
//
//      // randomize some CC parameters based on which note was triggered
//      switch (drumNotes[note])
//      {
//        case LO_TOM:
//        case HI_TOM:
//          // tom decay
//          MIDI.sendControlChange(56, random(127), 10);
//          break;
//        case C_HAT:
//          // closed hat decay and grain
//          MIDI.sendControlChange(57, random(127), 10);
//          MIDI.sendControlChange(59, random(127), 10);
//          break;
//        case O_HAT:
//          // open hat decay and grain
//          MIDI.sendControlChange(58, random(127), 10);
//          MIDI.sendControlChange(59, random(127), 10);
//          break;
//        case CLAP:
//          // pcm speed
//          MIDI.sendControlChange(50, random(127), 10);
//        case CLAVES:
//          // pcm speed
//          MIDI.sendControlChange(51, random(127), 10);
//        case AGOGO:
//          // pcm speed
//          MIDI.sendControlChange(52, random(127), 10);
//        case CRASH:
//          // pcm speed
//          MIDI.sendControlChange(53, random(127), 10);
//      }
//    }

    // part levels are mapped to cc 40-48, so we can use note as our offset
    MIDI.sendControlChange(40 + note, velocity, 10); // CC for part level
    MIDI.sendNoteOn(drumNotes[note], velocity, 10); // Note on for drum part
    MIDI.sendNoteOff(drumNotes[note], 0, 10);
  }
}

void loop()
{
  // read in values from our knobs and step count control
  readStepDivision();
  readTempo();
  readProbability();
  readVelocity();

  MIDI.read();

}

// Calculates the delay time between beats based on knob position, from 20 to 200 bpm
void readTempo()
{
  int tempoPot = analogRead(TEMPO);  // read in value from 0-1023
  bpm = ((tempoPot * 200.0) / 1024.0) + 20.0; // shift that range to 20 - 200 (bpm)

  // 60 seconds divided by tempo (bpm) will give us number of seconds between each quarter note, e.g. 60sec / 120bpm = 0.5sec between beats
  // Normally we would multiply that by 1000 to convert to milliseconds, but since we want to convert our quarter note timing to 16th notes,
  // we'll only mulitply by 250 (or 125 if we're doing double time, e.g. 32nd notes).
  tempoDelay = (60.0 / bpm) * (1000 / stepsPerQuarter);
}

// This sets our multiplication value for 16th notes or 32nd notes
//void readDoubleTime()
//{
//  if (digitalRead(DTPIN) == LOW)
//  {
//    sixteenthNote = 125.0;
//  } else {
//    sixteenthNote = 250.0;
//  }
//}

// Simple read in from 0-1023
void readProbability()
{
  probability = analogRead(PROBABILITY);
}

// read our velocity controls and convert to a range for randomization
void readVelocity()
{
  // get min and max velocity settings
  // bit shifting by 3 will convert 0-1024 into 0-127 ranges
  min_velocity = analogRead(VELOCITY_LOW) >> 3;
  max_velocity = analogRead(VELOCITY_HIGH) >> 3;
  // calculate range, but use 0 if negative range
  velocity_range = max(0, max_velocity - min_velocity);
  // in the loop we will be adding a random number in the range to the min velocity on each step
}

// read if using 8th, 16th, or 32nd note steps
void readStepDivision()
{
  if (digitalRead(EIGHTH) == LOW)
  {
    stepsPerQuarter = 2;
  } else if (digitalRead(THIRTYSECOND) == LOW) {
    stepsPerQuarter = 8;
  } else {
    stepsPerQuarter = 4;
  }
}

