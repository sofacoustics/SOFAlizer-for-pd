/*

IMU-Sine-Synth-Pd Example from Bela On Ur Head

by Becky Stewart 2017

See the bottom of this file for more details on how to use it.


This code is based on:
MrHeadTracker https://git.iem.at/DIY/MrHeadTracker
Adafruit BNO055 Sensor library https://github.com/adafruit/Adafruit_BNO055


This software is distributed under the GNU Lesser General Public License
(LGPL 3.0), available here: https://www.gnu.org/licenses/lgpl-3.0.txt

---------------------------------------------------------------
 ____  _____ _        _    
| __ )| ____| |      / \   
|  _ \|  _| | |     / _ \  
| |_) | |___| |___ / ___ \ 
|____/|_____|_____/_/   \_\

The platform for ultra-low latency audio and sensor processing

http://bela.io

A project of the Augmented Instruments Laboratory within the
Centre for Digital Music at Queen Mary University of London.
http://www.eecs.qmul.ac.uk/~andrewm

(c) 2016 Augmented Instruments Laboratory: Andrew McPherson,
  Astrid Bin, Liam Donovan, Christian Heinrichs, Robert Jack,
  Giulio Moro, Laurel Pardue, Victor Zappi. All rights reserved.

The Bela software is distributed under the GNU Lesser General Public License
(LGPL 3.0), available here: https://www.gnu.org/licenses/lgpl-3.0.txt
*/

/*
 *	USING A CUSTOM RENDER.CPP FILE FOR PUREDATA PATCHES - LIBPD
 *  ===========================================================
 *  ||                                                       ||
 *  || OPEN THE ENCLOSED _main.pd PATCH FOR MORE INFORMATION ||
 *  || ----------------------------------------------------- ||
 *  ===========================================================
 */

#include <Bela.h>
#include <DigitalChannelManager.h>
#include <cmath>
#include <I2c_Codec.h>
#include <PRU.h>
#include <stdio.h>
#include <libpd/z_libpd.h>
extern "C" {
#include <libpd/s_stuff.h>
};
#include <UdpServer.h>
#include <Midi.h>
#include <Scope.h>
#include <string>
#include <sstream>
// #include <rtdk.h>
#include "Bela_BNO055.h"

//************ Added for BNO055 based head-tracking ***************
// Change this to change how often the BNO055 IMU is read (in Hz)
int readInterval = 100;

I2C_BNO055 bno; // IMU sensor object
int buttonPin = 1; // calibration button pin
int lastButtonValue = 0; // using a pulldown resistor
int buttonBypass = 4; // bypass pin
int lastBypassValue = 0; // using a pulldown resistor
int buttonSofas = 2; // button to step through list of sofa files
int lastSofasValue = 0; // using a pulldown resistor
// Quaternions and Vectors
imu::Quaternion gCal, gCalLeft, gCalRight, gIdleConj = {1, 0, 0, 0};
imu::Quaternion qGravIdle, qGravCal, quat, steering, qRaw;

imu::Vector<3> gRaw;         
imu::Vector<3> gGravIdle, gGravCal;
imu::Vector<3> ypr; //yaw pitch and roll angles

int sofasState = 0; // state machine variable for switching sofa files
int bypassState = 0; // state machine variable for bypass
int calibrationState = 0; // state machine variable for calibration
int setForward = 0; // flag for setting forward orientation

// variables handling threading
AuxiliaryTask i2cTask;		// Auxiliary task to read I2C
AuxiliaryTask gravityNeutralTask;		// Auxiliary task to read gravity from I2C
AuxiliaryTask gravityDownTask;		// Auxiliary task to read gravity from I2C

int readCount = 0;			// How long until we read again...
int readIntervalSamples = 0; // How many samples between reads

int printThrottle = 0; // used to limit printing frequency

// function declarations
void readIMU(void*);
void getNeutralGravity(void*);
void getDownGravity(void*);
void calibrate();
void resetOrientation();


void Bela_userSettings(BelaInitSettings *settings)
{
	settings->uniformSampleRate = 1;
	settings->interleave = 0;
	settings->analogOutputsPersist = 0;
}

//*************************************


float* gInBuf;
float* gOutBuf;
#define PARSE_MIDI
static std::vector<Midi*> midi;
std::vector<std::string> gMidiPortNames;

void dumpMidi()
{
	if(midi.size() == 0)
	{
		printf("No MIDI device enabled\n");
		return;
	}
	printf("The following MIDI devices are enabled:\n");
	printf("%4s%20s %3s %3s %s\n",
			"Num",
			"Name",
			"In",
			"Out",
			"Pd channels"
	      );
	for(unsigned int n = 0; n < midi.size(); ++n)
	{
		printf("[%2d]%20s %3s %3s (%d-%d)\n", 
			n,
			gMidiPortNames[n].c_str(),
			midi[n]->isInputEnabled() ? "x" : "_",
			midi[n]->isOutputEnabled() ? "x" : "_",
			n * 16 + 1,
			n * 16 + 16
		);
	}
}

Midi* openMidiDevice(std::string name, bool verboseSuccess = false, bool verboseError = false)
{
	Midi* newMidi;
	newMidi = new Midi();
	newMidi->readFrom(name.c_str());
	newMidi->writeTo(name.c_str());
#ifdef PARSE_MIDI
	newMidi->enableParser(true);
#else
	newMidi->enableParser(false);
#endif /* PARSE_MIDI */
	if(newMidi->isOutputEnabled())
	{
		if(verboseSuccess)
			printf("Opened MIDI device %s as output\n", name.c_str());
	}
	if(newMidi->isInputEnabled())
	{
		if(verboseSuccess)
			printf("Opened MIDI device %s as input\n", name.c_str());
	}
	if(!newMidi->isInputEnabled() && !newMidi->isOutputEnabled())
	{
		if(verboseError)
			fprintf(stderr, "Failed to open  MIDI device %s\n", name.c_str());
		return nullptr;
	} else {
		return newMidi;
	}
}

static unsigned int getPortChannel(int* channel){
	unsigned int port = 0;
	while(*channel > 16){
		*channel -= 16;
		port += 1;
	}
	if(port >= midi.size()){
		// if the port number exceeds the number of ports available, send out
		// of the first port 
		rt_fprintf(stderr, "Port out of range, using port 0 instead\n");
		port = 0;
	}
	return port;
}

void Bela_MidiOutNoteOn(int channel, int pitch, int velocity) {
	int port = getPortChannel(&channel);
	rt_printf("noteout _ port: %d, channel: %d, pitch: %d, velocity %d\n", port, channel, pitch, velocity);
	midi[port]->writeNoteOn(channel, pitch, velocity);
}

void Bela_MidiOutControlChange(int channel, int controller, int value) {
	int port = getPortChannel(&channel);
	rt_printf("ctlout _ port: %d, channel: %d, controller: %d, value: %d\n", port, channel, controller, value);
	midi[port]->writeControlChange(channel, controller, value);
}

void Bela_MidiOutProgramChange(int channel, int program) {
	int port = getPortChannel(&channel);
	rt_printf("pgmout _ port: %d, channel: %d, program: %d\n", port, channel, program);
	midi[port]->writeProgramChange(channel, program);
}

void Bela_MidiOutPitchBend(int channel, int value) {
	int port = getPortChannel(&channel);
	rt_printf("bendout _ port: %d, channel: %d, value: %d\n", port, channel, value);
	midi[port]->writePitchBend(channel, value);
}

void Bela_MidiOutAftertouch(int channel, int pressure){
	int port = getPortChannel(&channel);
	rt_printf("touchout _ port: %d, channel: %d, pressure: %d\n", port, channel, pressure);
	midi[port]->writeChannelPressure(channel, pressure);
}

void Bela_MidiOutPolyAftertouch(int channel, int pitch, int pressure){
	int port = getPortChannel(&channel);
	rt_printf("polytouchout _ port: %d, channel: %d, pitch: %d, pressure: %d\n", port, channel, pitch, pressure);
	midi[port]->writePolyphonicKeyPressure(channel, pitch, pressure);
}

void Bela_MidiOutByte(int port, int byte){
	rt_printf("port: %d, byte: %d\n", port, byte);
	if(port > (int)midi.size()){
		// if the port is out of range, redirect to the first port.
		rt_fprintf(stderr, "Port out of range, using port 0 instead\n");
		port = 0;
	}
	midi[port]->writeOutput(byte);
}

void Bela_printHook(const char *received){
	rt_printf("%s", received);
}

static DigitalChannelManager dcm;

void sendDigitalMessage(bool state, unsigned int delay, void* receiverName){
	libpd_float((char*)receiverName, (float)state);
//	rt_printf("%s: %d\n", (char*)receiverName, state);
}

#define LIBPD_DIGITAL_OFFSET 11 // digitals are preceded by 2 audio and 8 analogs (even if using a different number of analogs)

void Bela_messageHook(const char *source, const char *symbol, int argc, t_atom *argv){
	if(strcmp(source, "bela_setMidi") == 0){
		int num[3] = {0, 0, 0};
		for(int n = 0; n < argc && n < 3; ++n)
		{
			if(!libpd_is_float(&argv[n]))
			{
				fprintf(stderr, "Wrong format for Bela_setMidi, expected:[hw 1 0 0(");
				return;
			}
			num[n] = libpd_get_float(&argv[n]);
		}
		std::ostringstream deviceName;
		deviceName << symbol << ":" << num[0] << "," << num[1] << "," << num[2];
		printf("Adding Midi device: %s\n", deviceName.str().c_str());
		Midi* newMidi = openMidiDevice(deviceName.str(), false, true);
		if(newMidi)
		{
			midi.push_back(newMidi);
			gMidiPortNames.push_back(deviceName.str());
		}
		dumpMidi();
		return;
	}
	if(strcmp(source, "bela_setDigital") == 0){
		// symbol is the direction, argv[0] is the channel, argv[1] (optional)
		// is signal("sig" or "~") or message("message", default) rate
		bool isMessageRate = true; // defaults to message rate
		bool direction = 0; // initialize it just to avoid the compiler's warning
		bool disable = false;
		if(strcmp(symbol, "in") == 0){
			direction = INPUT;
		} else if(strcmp(symbol, "out") == 0){
			direction = OUTPUT;
		} else if(strcmp(symbol, "disable") == 0){
			disable = true;
		} else {
			return;
		}
		if(argc == 0){
			return;
		} else if (libpd_is_float(&argv[0]) == false){
			return;
		}
		int channel = libpd_get_float(&argv[0]) - LIBPD_DIGITAL_OFFSET;
		if(disable == true){
			dcm.unmanage(channel);
			return;
		}
		if(argc >= 2){
			t_atom* a = &argv[1];
			if(libpd_is_symbol(a)){
				char *s = libpd_get_symbol(a);
				if(strcmp(s, "~") == 0  || strncmp(s, "sig", 3) == 0){
					isMessageRate = false;
				}
			}
		}
		dcm.manage(channel, direction, isMessageRate);
		return;
	}
}

void Bela_floatHook(const char *source, float value){

	// let's make this as optimized as possible for built-in digital Out parsing
	// the built-in digital receivers are of the form "bela_digitalOutXX" where XX is between 11 and 26
	static int prefixLength = 15; // strlen("bela_digitalOut")
	if(strncmp(source, "bela_digitalOut", prefixLength)==0){
		if(source[prefixLength] != 0){ //the two ifs are used instead of if(strlen(source) >= prefixLength+2)
			if(source[prefixLength + 1] != 0){
				// quickly convert the suffix to integer, assuming they are numbers, avoiding to call atoi
				int receiver = ((source[prefixLength] - 48) * 10);
				receiver += (source[prefixLength+1] - 48);
				unsigned int channel = receiver - 11; // go back to the actual Bela digital channel number
				if(channel < 16){ //16 is the hardcoded value for the number of digital channels
					dcm.setValue(channel, value);
				}
			}
		}
	}
}

char receiverNames[16][21]={
	{"bela_digitalIn11"},{"bela_digitalIn12"},{"bela_digitalIn13"},{"bela_digitalIn14"},{"bela_digitalIn15"},
	{"bela_digitalIn16"},{"bela_digitalIn17"},{"bela_digitalIn18"},{"bela_digitalIn19"},{"bela_digitalIn20"},
	{"bela_digitalIn21"},{"bela_digitalIn22"},{"bela_digitalIn23"},{"bela_digitalIn24"},{"bela_digitalIn25"},
	{"bela_digitalIn26"}
};

static unsigned int gAnalogChannelsInUse;
static unsigned int gLibpdBlockSize;
// 2 audio + (up to)8 analog + (up to) 16 digital + 4 scope outputs
static const unsigned int gChannelsInUse = 30;
//static const unsigned int gFirstAudioChannel = 0;
static const unsigned int gFirstAnalogInChannel = 2;
static const unsigned int gFirstAnalogOutChannel = 2;
static const unsigned int gFirstDigitalChannel = 10;
static const unsigned int gFirstScopeChannel = 26;
static char multiplexerArray[] = {"bela_multiplexer"};
static int multiplexerArraySize = 0;
static bool pdMultiplexerActive = false;

#ifdef PD_THREADED_IO
void fdLoop(void* arg){
	//t_pdinstance* pd_that = (t_pdinstance*)arg;
	while(!gShouldStop){
		sys_doio();
		usleep(3000);
	}
}
#endif /* PD_THREADED_IO */

Scope scope;
unsigned int gScopeChannelsInUse = 4;
float* gScopeOut;
void* gPatch;
bool gDigitalEnabled = 0;

// setup() is called once before the audio rendering starts.
// Use it to perform any initialisation and allocation which is dependent
// on the period size or sample rate.
//
// userData holds an opaque pointer to a data structure that was passed
// in from the call to initAudio().
//
// Return true on success; returning false halts the program.

bool setup(BelaContext *context, void *userData)
{
	// Check Pd's version
	int major, minor, bugfix;
	sys_getversion(&major, &minor, &bugfix);
	printf("Running Pd %d.%d-%d\n", major, minor, bugfix);
	// We requested in Bela_userSettings() to have uniform sampling rate for audio
	// and analog and non-interleaved buffers.
	// So let's check this actually happened
	if(context->analogSampleRate != context->audioSampleRate)
	{
		fprintf(stderr, "The sample rate of analog and audio must match. Try running with --uniform-sample-rate\n");
		return false;
	}
	if(context->flags & BELA_FLAG_INTERLEAVED)
	{
		fprintf(stderr, "The audio and analog channels must be interleaved.\n");
		return false;
	}

	if(context->digitalFrames > 0 && context->digitalChannels > 0)
		gDigitalEnabled = 1;

	// add here other devices you need 
	gMidiPortNames.push_back("hw:1,0,0");
	//gMidiPortNames.push_back("hw:0,0,0");
	//gMidiPortNames.push_back("hw:1,0,1");

	scope.setup(gScopeChannelsInUse, context->audioSampleRate);
	gScopeOut = new float[gScopeChannelsInUse];

	// Check first of all if the patch file exists. Will actually open it later.
	char file[] = "_main.pd";
	char folder[] = "./";
	unsigned int strSize = strlen(file) + strlen(folder) + 1;
	char* str = (char*)malloc(sizeof(char) * strSize);
	snprintf(str, strSize, "%s%s", folder, file);
	if(access(str, F_OK) == -1 ) {
		printf("Error file %s/%s not found. The %s file should be your main patch.\n", folder, file, file);
		return false;
	}
	free(str);
	if(//context->analogInChannels != context->analogOutChannels ||
			context->audioInChannels != context->audioOutChannels){
		fprintf(stderr, "This project requires the number of inputs and the number of outputs to be the same\n");
		return false;
	}
	// analog setup
	gAnalogChannelsInUse = context->analogInChannels;

	// digital setup
	if(gDigitalEnabled)
	{
		dcm.setCallback(sendDigitalMessage);
		if(context->digitalChannels > 0){
			for(unsigned int ch = 0; ch < context->digitalChannels; ++ch){
				dcm.setCallbackArgument(ch, receiverNames[ch]);
			}
		}
	}

	for(unsigned int n = 0; n < gMidiPortNames.size(); ++n)
	{
	}
	unsigned int n = 0;
	while(n < gMidiPortNames.size())
	{
		Midi* newMidi = openMidiDevice(gMidiPortNames[n], false, false);
		if(newMidi)
		{
			midi.push_back(newMidi);
			++n;
		} else {
			gMidiPortNames.erase(gMidiPortNames.begin() + n);
		}
	}
	dumpMidi();

	// check that we are not running with a blocksize smaller than gLibPdBlockSize
	gLibpdBlockSize = libpd_blocksize();
	if(context->audioFrames < gLibpdBlockSize){
		fprintf(stderr, "Error: minimum block size must be %d\n", gLibpdBlockSize);
		return false;
	}

	// set hooks before calling libpd_init
	libpd_set_printhook(Bela_printHook);
	libpd_set_floathook(Bela_floatHook);
	libpd_set_messagehook(Bela_messageHook);
	libpd_set_noteonhook(Bela_MidiOutNoteOn);
	libpd_set_controlchangehook(Bela_MidiOutControlChange);
	libpd_set_programchangehook(Bela_MidiOutProgramChange);
	libpd_set_pitchbendhook(Bela_MidiOutPitchBend);
	libpd_set_aftertouchhook(Bela_MidiOutAftertouch);
	libpd_set_polyaftertouchhook(Bela_MidiOutPolyAftertouch);
	libpd_set_midibytehook(Bela_MidiOutByte);

	//initialize libpd. This clears the search path
	libpd_init();
	//Add the current folder to the search path for externals
	libpd_add_to_search_path(".");
	libpd_add_to_search_path("../pd-externals");

	libpd_init_audio(gChannelsInUse, gChannelsInUse, context->audioSampleRate);
	gInBuf = get_sys_soundin();
	gOutBuf = get_sys_soundout();

	// start DSP:
	// [; pd dsp 1(
	libpd_start_message(1);
	libpd_add_float(1.0f);
	libpd_finish_message("pd", "dsp");

	// Bind your receivers here
	libpd_bind("bela_digitalOut11");
	libpd_bind("bela_digitalOut12");
	libpd_bind("bela_digitalOut13");
	libpd_bind("bela_digitalOut14");
	libpd_bind("bela_digitalOut15");
	libpd_bind("bela_digitalOut16");
	libpd_bind("bela_digitalOut17");
	libpd_bind("bela_digitalOut18");
	libpd_bind("bela_digitalOut19");
	libpd_bind("bela_digitalOut20");
	libpd_bind("bela_digitalOut21");
	libpd_bind("bela_digitalOut22");
	libpd_bind("bela_digitalOut23");
	libpd_bind("bela_digitalOut24");
	libpd_bind("bela_digitalOut25");
	libpd_bind("bela_digitalOut26");
	libpd_bind("bela_setDigital");
	libpd_bind("bela_setMidi");

	// open patch:
	gPatch = libpd_openfile(file, folder);
	if(gPatch == NULL){
		printf("Error: file %s/%s is corrupted.\n", folder, file); 
		return false;
	}

	// If the user wants to use the multiplexer capelet,
	// the patch will have to contain an array called "bela_multiplexer"
	// and a receiver [r bela_multiplexerChannels]
	if(context->multiplexerChannels > 0 && libpd_arraysize(multiplexerArray) >= 0){
		pdMultiplexerActive = true;
		multiplexerArraySize = context->multiplexerChannels * context->analogInChannels;
		// [; bela_multiplexer ` multiplexerArraySize` resize(
		libpd_start_message(1);
		libpd_add_float(multiplexerArraySize);
		libpd_finish_message(multiplexerArray, "resize");
		// [; bela_multiplexerChannels `context->multiplexerChannels`(
		libpd_float("bela_multiplexerChannels", context->multiplexerChannels);
	}

	// Tell Pd that we will manage the io loop,
	// and we do so in an Auxiliary Task
#ifdef PD_THREADED_IO
	sys_dontmanageio(1);
	AuxiliaryTask fdTask;
	fdTask = Bela_createAuxiliaryTask(fdLoop, 50, "libpd-fdTask", (void*)pd_this);
	Bela_scheduleAuxiliaryTask(fdTask);
#endif /* PD_THREADED_IO */

	//************ Added for BNO055 based head-tracking *****************
	if(!bno.begin()) {
		rt_printf("Error initialising BNO055\n");
		return false;
	}
	
	rt_printf("Initialised BNO055\n");
	
	// use external crystal for better accuracy
  	bno.setExtCrystalUse(true);
  	
	// get the system status of the sensor to make sure everything is ok
	uint8_t sysStatus, selfTest, sysError;
  	bno.getSystemStatus(&sysStatus, &selfTest, &sysError);
	rt_printf("System Status: %d (0 is Idle)   Self Test: %d (15 is all good)   System Error: %d (0 is no error)\n", sysStatus, selfTest, sysError);

	
	// set sensor reading in a separate thread
	// so it doesn't interfere with the audio processing
	i2cTask = Bela_createAuxiliaryTask(&readIMU, 5, "bela-bno");
	readIntervalSamples = context->audioSampleRate / readInterval;
	
	gravityNeutralTask = Bela_createAuxiliaryTask(&getNeutralGravity, 5, "bela-neu-gravity");
	gravityDownTask = Bela_createAuxiliaryTask(&getDownGravity, 5, "bela-down-gravity");
	
	// set up button pin
	pinMode(context, 0, buttonPin, INPUT); 
	// set up bypass pin
	pinMode(context, 0, buttonBypass, INPUT);
	// set up sofas pin
	pinMode(context, 0, buttonSofas, INPUT);

	//******************************
	
	return true;
}

// render() is called regularly at the highest priority by the audio engine.
// Input and output are given from the audio hardware and the other
// ADCs and DACs (if available). If only audio is available, numAnalogFrames
// will be 0.

void render(BelaContext *context, void *userData)
{
	int num;
#ifdef PARSE_MIDI
	for(unsigned int port = 0; port < midi.size(); ++port){
		while((num = midi[port]->getParser()->numAvailableMessages()) > 0){
			static MidiChannelMessage message;
			message = midi[port]->getParser()->getNextChannelMessage();
			rt_printf("On port %d (%s): ", port, gMidiPortNames[port].c_str());
			message.prettyPrint(); // use this to print beautified message (channel, data bytes)
			switch(message.getType()){
				case kmmNoteOn:
				{
					int noteNumber = message.getDataByte(0);
					int velocity = message.getDataByte(1);
					int channel = message.getChannel();
					libpd_noteon(channel + port * 16, noteNumber, velocity);
					break;
				}
				case kmmNoteOff:
				{
					/* PureData does not seem to handle noteoff messages as per the MIDI specs,
					 * so that the noteoff velocity is ignored. Here we convert them to noteon
					 * with a velocity of 0.
					 */
					int noteNumber = message.getDataByte(0);
	//				int velocity = message.getDataByte(1); // would be ignored by Pd
					int channel = message.getChannel();
					libpd_noteon(channel + port * 16, noteNumber, 0);
					break;
				}
				case kmmControlChange:
				{
					int channel = message.getChannel();
					int controller = message.getDataByte(0);
					int value = message.getDataByte(1);
					libpd_controlchange(channel + port * 16, controller, value);
					break;
				}
				case kmmProgramChange:
				{
					int channel = message.getChannel();
					int program = message.getDataByte(0);
					libpd_programchange(channel + port * 16, program);
					break;
				}
				case kmmPolyphonicKeyPressure:
				{
					int channel = message.getChannel();
					int pitch = message.getDataByte(0);
					int value = message.getDataByte(1);
					libpd_polyaftertouch(channel + port * 16, pitch, value);
					break;
				}
				case kmmChannelPressure:
				{
					int channel = message.getChannel();
					int value = message.getDataByte(0);
					libpd_aftertouch(channel + port * 16, value);
					break;
				}
				case kmmPitchBend:
				{
					int channel = message.getChannel();
					int value =  ((message.getDataByte(1) << 7)| message.getDataByte(0)) - 8192;
					libpd_pitchbend(channel + port * 16, value);
					break;
				}
				case kmmSystem:
				// currently Bela only handles sysrealtime, and it does so pretending it is a channel message with no data bytes, so we have to re-assemble the status byte
				{
					int channel = message.getChannel();
					int status = message.getStatusByte();
					int byte = channel | status;
					libpd_sysrealtime(port, byte);
					break;
				}
				case kmmNone:
				case kmmAny:
					break;
			}
		}
	}
#else
	int input;
	for(unsigned int port = 0; port < NUM_MIDI_PORTS; ++port){
		while((input = midi[port].getInput()) >= 0){
			libpd_midibyte(port, input);
		}
	}
#endif /* PARSE_MIDI */
	unsigned int numberOfPdBlocksToProcess = context->audioFrames / gLibpdBlockSize;

	// Remember: we have non-interleaved buffers and the same sampling rate for
	// analogs, audio and digitals
	for(unsigned int tick = 0; tick < numberOfPdBlocksToProcess; ++tick)
	{
		//audio input
		for(int n = 0; n < context->audioInChannels; ++n)
		{
			memcpy(
				gInBuf + n * gLibpdBlockSize,
				context->audioIn + tick * gLibpdBlockSize + n * context->audioFrames, 
				sizeof(context->audioIn[0]) * gLibpdBlockSize
			);
		}

		// analog input
		for(int n = 0; n < context->analogInChannels; ++n)
		{
			memcpy(
				gInBuf + gLibpdBlockSize * gFirstAnalogInChannel + n * gLibpdBlockSize,
				context->analogIn + tick * gLibpdBlockSize + n * context->analogFrames, 
				sizeof(context->analogIn[0]) * gLibpdBlockSize
			);
		}
		// multiplexed analog input
		if(pdMultiplexerActive)
		{
			// we do not disable regular analog inputs if muxer is active, because user may have bridged them on the board and
			// they may be using half of them at a high sampling-rate
			static int lastMuxerUpdate = 0;
			if(++lastMuxerUpdate == multiplexerArraySize){
				lastMuxerUpdate = 0;
				libpd_write_array(multiplexerArray, 0, (float *const)context->multiplexerAnalogIn, multiplexerArraySize);
			}
		}

		unsigned int digitalFrameBase = gLibpdBlockSize * tick;
		unsigned int j;
		unsigned int k;
		float* p0;
		float* p1;
		// digital input
		if(gDigitalEnabled)
		{
			// digital in at message-rate
			dcm.processInput(&context->digital[digitalFrameBase], gLibpdBlockSize);

			// digital in at signal-rate
			for (j = 0, p0 = gInBuf; j < gLibpdBlockSize; j++, p0++) {
				unsigned int digitalFrame = digitalFrameBase + j;
				for (k = 0, p1 = p0 + gLibpdBlockSize * gFirstDigitalChannel;
						k < 16; ++k, p1 += gLibpdBlockSize) {
					if(dcm.isSignalRate(k) && dcm.isInput(k)){ // only process input channels that are handled at signal rate
						*p1 = digitalRead(context, digitalFrame, k);
					}
				}
			}
		}
		
		//************ Added for BNO055 based head-tracking ***************

		// this schedules the imu sensor readings
		if(readCount >= readIntervalSamples) {
			readCount = 0;
			Bela_scheduleAuxiliaryTask(i2cTask);
		}
		readCount += gLibpdBlockSize;
		
		//read the value of the button
		int bypassValue = digitalRead(context, 0, buttonBypass); 

		// if button wasn't pressed before and is pressed now
		if( bypassValue != lastBypassValue && bypassValue == 1 ){
			if (bypassState == 0) {
				bypassState = 1;
			}
			else {
				(bypassState = 0);
			}
		}
		lastBypassValue = bypassValue;
		libpd_float("bypass", bypassState);
		
		//read the value of the button
		int sofasValue = digitalRead(context, 0, buttonSofas); 

		// if button wasn't pressed before and is pressed now
		if( sofasValue != lastSofasValue && sofasValue == 1 ){
			if (sofasState == 0) {
				sofasState = 1;
			}
			else {
				(sofasState = 0);
			}
		}
		lastSofasValue = sofasValue;
		libpd_float("bno-sofa", sofasState);
		
		//switch(bypassState) {
		//case 0: 
			// send IMU values to Pd
			libpd_float("bno-yaw", ypr[0]);
			libpd_float("bno-pitch", ypr[1]);
			libpd_float("bno-roll", ypr[2]);
			//break;
		//case 1:
			// send Bypass values to Pd
			//libpd_float("bno-yaw", 0.5);
			//libpd_float("bno-pitch", 0.5);
			//libpd_float("bno-roll", 0.5);
			//break;
		//}

		//read the value of the button
		int buttonValue = digitalRead(context, 0, buttonPin); 

		// if button wasn't pressed before and is pressed now
		if( buttonValue != lastButtonValue && buttonValue == 1 ){
			// then run calibration to set looking forward (gGravIdle) 
			// and looking down (gGravCal)
			switch(calibrationState) {
			case 0: // first time button was pressed
				setForward = 1;
				// run task to get gravity values when sensor in neutral position
				Bela_scheduleAuxiliaryTask(gravityNeutralTask);
				calibrationState = 1;	// progress calibration state
				break;
			case 1: // second time button was pressed
				// run task to get gravity values when sensor 'looking down' (for head-tracking) 
		 		Bela_scheduleAuxiliaryTask(gravityDownTask);
				calibrationState = 0; // reset calibration state for next time
				break;
			} 
		}
		lastButtonValue = buttonValue;

		libpd_process_sys(); // process the block

		// digital outputs
		if(gDigitalEnabled)
		{
			// digital out at signal-rate
			for (j = 0, p0 = gOutBuf; j < gLibpdBlockSize; ++j, ++p0) {
				unsigned int digitalFrame = (digitalFrameBase + j);
				for (k = 0, p1 = p0  + gLibpdBlockSize * gFirstDigitalChannel;
					k < context->digitalChannels; k++, p1 += gLibpdBlockSize)
				{
					if(dcm.isSignalRate(k) && dcm.isOutput(k)){ // only process output channels that are handled at signal rate
					digitalWriteOnce(context, digitalFrame, k, *p1 > 0.5);
					}
				}
			}

			// digital out at message-rate
			dcm.processOutput(&context->digital[digitalFrameBase], gLibpdBlockSize);
		}

		// scope output
		for (j = 0, p0 = gOutBuf; j < gLibpdBlockSize; ++j, ++p0) {
			for (k = 0, p1 = p0 + gLibpdBlockSize * gFirstScopeChannel; k < gScopeChannelsInUse; k++, p1 += gLibpdBlockSize) {
				gScopeOut[k] = *p1;
			}
			scope.log(gScopeOut[0], gScopeOut[1], gScopeOut[2], gScopeOut[3]);
		}
	
		
		//************ 

		// audio output
		for(int n = 0; n < context->audioInChannels; ++n)
		{
			memcpy(
				context->audioOut + tick * gLibpdBlockSize + n * context->audioFrames, 
				gOutBuf + n * gLibpdBlockSize,
				sizeof(context->audioOut[0]) * gLibpdBlockSize
			);
		}

		//analog output
		for(int n = 0; n < context->analogOutChannels; ++n)
		{
			memcpy(
				context->analogOut + tick * gLibpdBlockSize + n * context->analogFrames, 
				gOutBuf + gLibpdBlockSize * gFirstAnalogOutChannel + n * gLibpdBlockSize,
				sizeof(context->analogOut[0]) * gLibpdBlockSize
			);
		}
	}
}




// Auxiliary task to read from the I2C board
void readIMU(void*)
{
	// get calibration status
	uint8_t sys, gyro, accel, mag;
	bno.getCalibration(&sys, &gyro, &accel, &mag);
	// status of 3 means fully calibrated
	//rt_printf("CALIBRATION STATUSES\n");
	//rt_printf("System: %d   Gyro: %d Accel: %d  Mag: %d\n", sys, gyro, accel, mag);
	
	// quaternion data routine from MrHeadTracker
  	imu::Quaternion qRaw = bno.getQuat(); //get sensor raw quaternion data
  	
  	if( setForward ) {
  		gIdleConj = qRaw.conjugate(); // sets what is looking forward
  		setForward = 0; // reset flag so only happens once
  	}
		
  	steering = gIdleConj * qRaw; // calculate relative rotation data
  	quat = gCalLeft * steering; // transform it to calibrated coordinate system
  	quat = quat * gCalRight;

  	ypr = quat.toEuler(); // transform from quaternion to Euler
}

// Auxiliary task to read from the I2C board
void getNeutralGravity(void*) {
	// read in gravity value
  	imu::Vector<3> gravity = bno.getVector(I2C_BNO055::VECTOR_GRAVITY);
  	gravity = gravity.scale(-1);
  	gravity.normalize();
  	gGravIdle = gravity;
}

// Auxiliary task to read from the I2C board
void getDownGravity(void*) {
	// read in gravity value
  	imu::Vector<3> gravity = bno.getVector(I2C_BNO055::VECTOR_GRAVITY);
  	gravity = gravity.scale(-1);
  	gravity.normalize();
  	gGravCal = gravity;
  	// run calibration routine as we should have both gravity values
  	calibrate(); 
}

// calibration of coordinate system from MrHeadTracker
// see http://www.aes.org/e-lib/browse.cfm?elib=18567 for full paper
// describing algorithm
void calibrate() {
  	imu::Vector<3> g, gravCalTemp, x, y, z;
  	g = gGravIdle; // looking forward in neutral position
  
  	z = g.scale(-1); 
  	z.normalize();

  	gravCalTemp = gGravCal; // looking down
  	y = gravCalTemp.cross(g);
  	y.normalize();

  	x = y.cross(z);
  	x.normalize();

  	imu::Matrix<3> rot;
  	rot.cell(0, 0) = x.x();
  	rot.cell(1, 0) = x.y();
  	rot.cell(2, 0) = x.z();
  	rot.cell(0, 1) = y.x();
  	rot.cell(1, 1) = y.y();
  	rot.cell(2, 1) = y.z();
  	rot.cell(0, 2) = z.x();
  	rot.cell(1, 2) = z.y();
  	rot.cell(2, 2) = z.z();

  	gCal.fromMatrix(rot);

  	resetOrientation();
}

// from MrHeadTracker
// resets values used for looking forward
void resetOrientation() {
  	gCalLeft = gCal.conjugate();
  	gCalRight = gCal;
}


// cleanup() is called once at the end, after the audio has stopped.
// Release any resources that were allocated in setup().

void cleanup(BelaContext *context, void *userData)
{
	for(auto a : midi)
	{
		delete a;
	}
	libpd_closefile(gPatch);
	delete [] gScopeOut;
}


/**
\imu-sine-sythn/render.cpp

Inertial Measurement Unit (IMU) Sensing with the BNO055
----------------------------------------------------------

This sketch allows you to hook up BNO055 IMU movement sensing device
to Bela, for example the Adafruit BNO055 breakout board.

To get this working with Bela you need to connect the breakout board to the I2C
terminal on the Bela board. See the Pin guide for details of which pin is which.

Connect a push button with a pull-down resistor to pin P8_08.

When running sketch, hold IMU in a neutral position and press the push button 
once. Tilt IMU down (if wearing as for head-tracking, look down) and press the 
push button a second time. The system is now calibrated. Calibration can be
run again at any time.

*/
