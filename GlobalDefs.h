// GlobalDefs.h -- Holds pound defines used throughout the program and a couple of header files that optionally get
// loaded depending on certain defines.  Put these things here instead on StdAfx.h to avoid lengthy recompiles.
#pragma once

#define GL_OFFSET 22.86
// Offsets for the center of rotation from the centroid of the
// platform in meters.
//#define CENTROID_OFFSET_X 0.0
//#define CENTROID_OFFSET_Y 0.0
//#define CENTROID_OFFSET_Z 0.90
//The global variable PLATFORM_ROT_CENTER and CUBE_ROT_CENTER is the replacement of CENTROID_OFFSET.

#define DATA_FRAME MoogFrame
#define CORE_CLASS MoogCom
#define CORE_CONSTRUCTOR MoogCom(mbcIP, mbcPort, localIP, localPort, useCustomTimer)
#define SET_DATA_FRAME ThreadSetAxesPositions
#define THREAD_GET_DATA_FRAME ThreadGetAxesPositions
#define GET_DATA_FRAME GetAxesPositions

// Load the debug libraries for the Debug build.
#ifdef _DEBUG
#pragma comment(lib, "MoogComd")
#else
#pragma comment(lib, "MoogCom")
#endif // #ifdef _DEBUG

#define DEBUG_DEFAULTS 0

#define SHOW_GL_WINDOW 1	// Show the OpenGL rendering window.
#define SWAP_TIMER 0		// Time the speed of SwapBuffers().
#define CUSTOM_TIMER 1		// Have the thread use a custom defined timer.
#define INSERT_BUMP 0		// Insert a manually generated bump into the movement.
#define USE_MATLAB 0		// Load Matlab crap.
#define USE_LOCALHOST 0		// Only communicate with the localhost.
#define DUAL_MONITORS 1		// Dual monitor support.
#define FLIP_MONITORS 0
#define SINGLE_CPU_MACHINE 0
#define CIRCLE_TEST 0
#define ESTOP 1				//For MotionParallax: #define USE_SIGNAL_BIT 1
#define USE_STEREO 1
#define RECORD_MODE 0
#define USE_ANALOG_OUT_BOARD 1
#define PCI_DIO_24H_PRESENT 1
//#define FIRST_PULSE_ONLY 0
#define SMALL_MONITOR 0
#define USE_MATLAB_RDX 0
#define VERBOSE_MODE 0		// Turns verbose mode on/off by default.
#define LISTEN_MODE 0		// Turns listen mode on/off by default.
#define DEF_LISTEN_MODE 1	// 0 = None, 1 = Tempo, 2 = Pipes
#define RECORD_MOVIE 0		// 1/0=On/Off recode movie by "Go" and "Stop" button

// Set various digital in/out info depending on whether we
// have a 24h board.
#if PCI_DIO_24H_PRESENT
#define PULSE_OUT_BOARDNUM m_PCI_DIO24_Object.DIO_board_num
#define ESTOP_IN_BOARDNUM m_PCI_DIO24_Object.DIO_board_num
#else
#define PULSE_OUT_BOARDNUM m_PCI_DIO48H_Object.DIO_board_num
#define ESTOP_IN_BOARDNUM m_PCI_DIO48H_Object.DIO_board_num
#endif

#define FIRS_TABLE_ROWS 500
#define FIRS_TABLE_COLS 41

#if DEBUG_DEFAULTS
#undef DEF_LISTEN_MODE
#define DEF_LISTEN_MODE 1
#undef LISTEN_MODE
#define LISTEN_MODE 0
#undef VERBOSE_MODE
#define VERBOSE_MODE 1
#undef USE_STEREO
#define USE_STEREO 1
#undef DUAL_MONITORS
#define DUAL_MONITORS 1
#undef USE_LOCALHOST
#define USE_LOCALHOST 1
#undef USE_MATLAB
#define USE_MATLAB 1
#undef SWAP_TIMER
#define SWAP_TIMER 0
#undef ESTOP
#define ESTOP 1
#undef USE_ANALOG_OUT_BOARD
#define USE_ANALOG_OUT_BOARD 1
#undef SINGLE_CPU_MACHINE
#define SINGLE_CPU_MACHINE 0
#endif

#define MAX_CONSOLE_LENGTH 100


/*
** Scales a distance (deg) value into an unsigned short to be sent to the PCI-DDA02/16 board.
**   Max unsigned short = 65535
**   Max magnitude of Moog movement = 50deg
**   Value that corresponds to "send 0 Volts" = 65535/2 = 32767.5
** End result is to map distance 50deg into unsigned short between 0 and 65535.
*/
#define DASCALE(val) (unsigned short)(((val)*32767.5/50)+32767.5)
														
// Conversion macros. // We don't use VIEW_DIST any more, so we define them as a function in GLWindow. - Johnny 12/15/08
//#define CMPERDEG  g_pList.GetVectorData("VIEW_DIST").at(0)*PI/180.0
//#define DISPTODEPTH(disp)  (g_pList.GetVectorData("VIEW_DIST").at(0)-((180.0*g_pList.GetVectorData("IO_DIST").at(0)*g_pList.GetVectorData("VIEW_DIST").at(0))/(-disp*PI*g_pList.GetVectorData("VIEW_DIST").at(0)+180.0*g_pList.GetVectorData("IO_DIST").at(0))))

// Moog
#include <MoogCom.h>
#include <NumericalMethods.h>

#if USE_MATLAB
// Matlab
#include <engine.h>
#endif

// This text file records the Moog system location
#define RIG_FILE "C:\\rig_id.txt"

// Define 2 Moog system in UR
enum
{
	MOOG_164,
	MOOG_157
};

#define LEFT_EYE 0
#define RIGHT_EYE 1
#define NUM_OF_DYN_OBJ 1
#define NUM_OF_STATIC_OBJ 10
#define MAX_DYN_OBJ_COPY_NUM 10

