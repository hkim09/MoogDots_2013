#pragma once

#include "GlobalDefs.h"
#include "GLWindow.h"
#include "ParameterList.h"
#include "CB_Tools.h"
#include "Affine4Matrix.h"
#include "GLToMovie.h"

using namespace std;

#define TINY_NUMBER 0.0001f
#define HEAVE_OFFSET 69.3028
#define SPEED_BUFFER_SIZE 31	// Size of the stop buffer for fixation breaks.

// The base of the exponential set of values used to pad
// an abrupt stop when the monkey breaks fixation.
#define EXP_BASE 0.85
#define EXP_BASE2 0.85

// Parameters of the diffence function.
#define LATERAL_POLE 0.099064
#define LATERAL_ZERO 0.03508
#define HEAVE_POLE 0.092322
#define HEAVE_ZERO 0.028394
#define SURGE_POLE 0.097584
#define SURGE_ZERO 0.033943

#define CONTROL_LOOP_TIME 32.0	// Maximum time in ms for the control loop to read continuously from Tempo.

#define SCAN_SIZE 4			// Number of channels to scan for analog input.

#define VOLTS2DEGS 36.0		// 180.0/5.0 degs/volts
#define VOLTS2METERS .1     // 0.5/5.0 m/volts

#define PIPE_BUFSIZE 4096

#define MODE_MOOG 0
#define MODE_OPENGL 1

#define MODE_GAUSSIAN 0
#define MODE_TRAPEZOID 1

class CMainFrame;

class MoogDotsCom : public CORE_CLASS
{
public:
	enum ConnectionType
	{
		None,
		Tempo,
		Pipes
	};

private:
	ConnectionType m_connectionType;
	OVERLAPPED m_overlappedEvent;		// Used for asynchronous pipes communication.
	HANDLE m_pipeHandle;				// Handle for pipe communication.
	char m_pipeBuff[PIPE_BUFSIZE];		// Buffer for pipe communication.
	SECURITY_DESCRIPTOR m_pipeSD;
	SECURITY_ATTRIBUTES m_pipeSA;
	queue<string> m_commandQueue;		// Queue for commands read from the pipe.

	CMainFrame *m_mainFrame;			// Pointer to the main frame window.
public:
	GLWindow *m_glWindow;				// Pointer to the OpenGL stimulus window.
private:
	nmMovementData m_data,				// Motion base movement information.
				 m_glData,				// GL scene translation movement information. Moog Coord
				 m_glDynObjData[NUM_OF_DYN_OBJ],	// Object translation movement information. Here we use OpenGL coord
				 m_rotData,				// Rotation data.
				 m_noise,				// Noise data.
				 m_filteredNoise,		// Filtered Noise.
				 m_fpData,
				 m_fpRotData,
				 m_glRotCenter;			// In translation plus rotation case, we need reset the rotation center each frame.
	vector<double> m_sendStamp,			// Time stamp right before sending a UDP packet.
				   m_receiveStamp,		// Time stamp right after receiving a UDP packet.
				   m_recordedLateral, m_recordedHeave, m_recordedSurge, m_interpLateral,
				   m_interpHeave, m_interpSurge, m_interpRotation, m_glRotData,
				   m_recordedYaw, m_recordedPitch, m_recordedRoll, m_recordedYawVel;
	vector<double> m_swapStamp;
	vector<int> m_stimOnData;				// 1/0=On/Off visual stimulus for each frame
	int m_stimOnOffIndex[2];				// record which frame start and stop showing visual stimulus
	LARGE_INTEGER m_freq;				// Frequency of the high resolution timer.
	bool m_glWindowExists,				// Indicates if the GLWindow was created or not.
		 m_isLibLoaded,
		 m_drawRegularFeedback,
		 m_verboseMode,					// Indicates if we have verbose output in the message console.
//#if CUSTOM_TIMER - Johnny 6/17/2007
		 m_doSyncPulse,					// Flag to send the sync pulse.
//#endif
		 m_setRotation;					// Flag to determine if we set the rotation values in the Compute function.
	HGLRC m_threadGLContext;			// Rendering context owned by the communication thread.
	double m_delay;
	int m_recordOffset,
		m_recordIndex,
		m_grabIndex;
	wxListBox *m_messageConsole;		// Message console where we print out useful runtime information.
	//static const double m_speedBuffer[SPEED_BUFFER_SIZE];
	double *m_speedBuffer;
	static const double m_speedBuffer2[SPEED_BUFFER_SIZE];
	DATA_FRAME m_previousPosition;
	nm3DDatum m_rotationVector;
	bool m_iocomplete;					// Flags the call to ReadFile().

	// Variables for generating the Gaussian analog output.
	vector<double> m_gaussianTrajectoryData,
		           m_interpStimulusData,
		           m_StimulusData;

	// Simulate accelerometer signal and then output by cbAOut()
	vector<double> m_accelerData, m_accelerLateral, m_accelerHeave, m_accelerSurge, m_accelerRotation, m_velocityData;

#if USE_MATLAB
	Engine *m_engine;					// Matlab engine
#endif

	// Tempo stuff.
	CCB_Tools m_PCI_DIO24_Object,
		      m_PCI_DIO48H_Object;

	int m_RDX_base_address;
	short m_tempoHandle,
		  m_tempoErr;
	char m_tempoBuffer[256];
	bool m_listenMode,
		 m_continuousMode;

	bool m_previousBitLow;				// Keeps track of what the previous stop bit was.

	CMatlabRDX *m_matlabRDX;

public:
	MoogDotsCom(CMainFrame *mainFrame, char *mbcIP, int mbcPort, char *localIP, int localPort, bool useCustomTimer);
	~MoogDotsCom();

	// Sets/Gets the connection type.
	void SetConnectionType(MoogDotsCom::ConnectionType ctype);
	ConnectionType GetConnectionType();

#if USE_MATLAB
	// Creates a new Matlab engine.
	void StartMatlab();

	// Closes the existing Matlab engine.
	void CloseMatlab();

	// Stuffs recorded data into Matlab.
	void StuffMatlab();
#endif

	// ************************************************************************ //
	//	void UpdateGLScene(bool doSwapBuffers)									//
	//		Updates the GL scene to reflect parameter list changes.				//
	//																			//
	//	Inputs: doSwapBuffers -- If true, the OpenGL scene is rendered and		//
	//							 SwapBuffers() is called.						//
	// ************************************************************************ //
	void UpdateGLScene(bool doSwapBuffers);

	// Calculates the Gaussian needed to move the motion base given a start
	// position, time, and end position.
	void CalculateGaussianMovement(DATA_FRAME *startFrame, double elevation,
								   double azimuth, double magnitude, double duration,
								   double sigma, bool doSecondMovement);

	// Calculates the trajectory for a Gaussian rotation.
	void CalculateRotationMovement(int mode=MODE_GAUSSIAN );

	// Calculates the trajectory for a sinusoidal movement.
	void CalculateSinusoidMovement();

	// Calculates the trajectory for a step velocity rotation (visual only).
	void CalculateStepVelocityMovement();

	// Calculates the trajectory for a continuous circular movement in the
	// horizontal plane.
	void CalculateCircleMovement();

	// Calculates a 2 interval trajectory.
	void Calculate2IntervalMovement();

	// Calculates a tilt translation trajectory.
	void CalculateTiltTranslationMovement();

	// Calculates a gabor trajectory.
	void CalculateGaborMovement();

	// Calculates Gaussian object movement
	void CalculateGaussianObjectMovement(int objNum);

	// Calculates Motion Parallax movement (sin movement)
	void CalculateSinMovement();

	// Calculates Sin(Motion Parallax) object movement
	void CalculateSinObjectMovement(int objNum);

	// Calculates Pseudo Depth of object movement
	// This function will depend on camera trajectory, so it must call after calculated Moog trajectory.
	void CalculatePseudoDepthObjectMovement(int objNum);

	// Calculates Constant speed object movement (Constant Retinal or Constant in the world)
	void CalculateConstSpeedObjectMovement(int objNum);

	// Calculate Trapezoid Velocity Object Movement
	void CalculateTrapezoidVelocityObjectMovement(int objNum);

	// Calculates the trajectory for a linear trapezoid velocity movement.
	void CalculateTrapezoidVelocityMovement();
	// Find 1D trapezoid velocity trajectory according to 
	// s -> displacement
	// t -> duration
	// t1 -> the start time of constant trapezoid velocity
	// t2 -> the end time of constant trapezoid velocity
	vector<double> FindTrapezoidVelocityTraj(double s, double t, double t1, double t2);

	// Calculates the trajectory for a Gaussian or Trapezoid translation plus rotation.
	void CalculateTranPlusRotMovement(int mode=MODE_GAUSSIAN);

	// Sets a pointer to the message console of the program.
	void SetConsolePointer(wxListBox *messageConsole);

	// ************************************************************************ //
	//	void InitTempo()														//
	//		Initializes Tempo stuff, like boards.								//
	// ************************************************************************ //
	void InitTempo();

	// Initializes the pipes.
	void InitPipes();

	// Wait for pipes connection.
	void ConnectPipes();

	// Closes the pipes.
	void ClosePipes();

	// ************************************************************************ //
	//	void ListenMode(bool value)												//
	//		Lets MoogDots listen for an external control source.				//
	//																			//
	//	Inputs: value -- true to use external control, false otherwise.			//
	// ************************************************************************ //
	void ListenMode(bool value);

	// ************************************************************************ //
	//	void ShowGLWindow(bool value)											//
	//		Shows or hides the GLWindow.										//
	//																			//
	//	Inputs: value == TRUE to show window, FALSE to hide.					//
	// ************************************************************************ //
	void ShowGLWindow(bool value);

	// ************************************************************************ //
	//	void SetVerbosity(bool value)											//
	//		Sets the verbosity of the message console.							//
	//																			//
	//	Inputs: value = true to turn verbosity on, false to turn off.			//
	// ************************************************************************ //
	void SetVerbosity(bool value);

	void OpenTempo();
	void CloseTempo();

// We only use this if we're using the built-in timer.
//#if !CUSTOM_TIMER - Johnny 6/17/07
public:
	// Returns true if vsync is enabled, false otherwise.
	bool VSyncEnabled();

	// Turns syncing to the SwapBuffer() call on and off.
	void SetVSyncState(bool enable);

private:
		// Sets up the pointers to the functions to modify the vsync for the program.
	void InitVSync();
//#endif - Johnny 6/17/07

private:
	// Overrides
	virtual void Compute();
	virtual void CustomTimer();
	virtual void ThreadInit();
	virtual void Control();
	virtual void Sync();
	virtual void ReceiveCompute();

	// Checks to see if the E-Stop bit has been set and takes any necessary actions.
	// Returns true if the E-Stop sequence was performed.
	bool CheckForEStop();

	//// Checks to see if parameters sent to the moog will make it exceed position,
	//// velocity, or acceration limits.  Returns true if all is OK, false if one of
	//// the limits was exceeded.
	//bool CheckMoogLimits(double position, 

	// ************************************************************************ //
	//	vector<double> convertPolar2Vector(double elevation, double azimuth,	//
	//									  double magnitude)						//
	//		Converts a movement defined by polar coordinates and a magnitude	//
	//		into a vector.  Angles need to be in radians.						//
	//																			//
	//	Inputs: elevation -- Elevation in radians.								//
	//			azimuth -- Azimuth in radians.									//
	//			magnitude -- Vector length.										//
	//	Returns: A double vector that contains the X, Y, and Z components.		//
	// ************************************************************************ //
	vector<double> convertPolar2Vector(double elevation, double azimuth,
									   double magnitude);

	// ************************************************************************ //
	//	double deg2rad(double deg)												//
	//		Converts a degree value into radians.								//
	//																			//
	//	Inputs: deg -- Degree value to convert.									//
	//	Returns: radian value.													//
	// ************************************************************************ //
	double deg2rad(double deg);

#if USE_MATLAB
	// Puts a double vector into the Matlab workspace.
	void stuffDoubleVector(vector<double> data, const char *variable);

	// Overload function
	void stuffDoubleVector(vector<int> data, const char *variable);
#endif

	// Creates a Frustum object based on the parameter list.
	Frustum createFrustum();

	// Creates a new StarField object based on the parameter list.
	StarField createStarField();

	// Compares two Frustums to see if they are equal.  Returns true if equal,
	// false otherwise.
	bool compareFrustums(Frustum a, Frustum b) const;

	// Compares two StarFields to see if they are equal.  Returns true if equal,
	// false otherwise.
	bool compareStarFields(StarField a, StarField b) const;

	// Generates predicted data.
	void GeneratePredictedData();

	// Generates predicted rotation data.
	void GeneratePredictedRotationData();

	// Difference function stuff used by the prediction function.  This stuff was
	// written by Greg, so all mistakes can be blamed on him, not me.
	vector<double> DifferenceFunc(double pole, double zero, vector<double> x, string title, bool stimulusOffset=false);

	// Replaces characters that are invalid in Matlab with regular characters.
	// This function is used to generate data structure names for our library.
	string replaceInvalidChars(string s);

	// Checks to see if a Tempo command has been sent.
	string checkTempo();

	// Checks to see if a Pipes command(s) has been sent.
	void CheckPipes(); 

	// Updates how the Moog should move, if at all.
	void UpdateMovement();

	// Creates the movement to move the platform from its current
	// position to some specified position.
	void MovePlatform(DATA_FRAME *destination);

	// Processes a command retrieved from Tempo or the pipes.
	void ProcessCommand(string command);

	// **** This is crap that James added.

	// Calculate 2 overlap interval trajectory
	void AddOverlap2Inerval(int mFirstEndIndex,int glFirstEndIndex,int overlapCnt);
public:

	// this functino only calculate Stimulus output, 
	//CalculateGaussianMovement function take a care of opengl window and platform movement
	//This function+CalculateGaussianMovement= Velocity output
	void CalculateStimulusOutput();

    // output
	void OutputStimulusCurve(int index);

	// Set port B - B0, B1 and B2
	void SetFirstPortBcbDOut();

	// for send signal to CED
	int stimAnalogOutput;
	double stimAnalogMult;
	double openGLsignal;
	bool m_customTimer;	// Replacement of the CUSTOM_TIMER defination
	int motionType;
	vector<double> objMotionType;
	double angularSpeed;
	vector<double> objDynLum[NUM_OF_DYN_OBJ];
#if RECORD_MOVIE
	bool recordMovie;
#endif

	DATA_FRAME FindStartFrame(); // Find start frame accroding to M_ORIGIN and  ROT_START_OFFSET for Moog platform only.

	// initial rendering
	void InitRender();

	// reture size of m_interpLateral
	int sizeOfInterp();

	//Return current data frame such that the heave value passed to us is based around zero.
	DATA_FRAME GetDataFrame();

	//Go back to origin with Camera movement
	void GoToOrigin();

private:
	// After we have onset and offset delay, we find the middle part of trajactory first.
	// Then we use followin function to create whole trajactory. 
	void CreateWholeTraj(nmMovementData *trajectory, double duration, double onsetDelay);
	void CreateWholeTraj(vector<double> *trajectory, double duration, double onsetDelay);

	// calculate rotation angles for each eye(camera) to simulation pursue case(only rotating FP) using OpenGL coord system.
	void CalEyeRotation(int whichEye, Point3 rotCenter, Vector3 rotVector, vector<double> rotAngles, Point3 FP);
	// calculate the quads masks for coving camera, so that find a small windows moving with pursue case(FP_ROTATE=2)
	void CalEyeMask(int whichEye, Point3 rotCenter, Vector3 rotVector, vector<double> rotAngles, Point3 FP);
	// calculate fake rotation that moves a huge object simulation of FP rotation.
	void CalFakeRotation(Point3 rotCenter, Vector3 rotVector, vector<double> rotAngles, Point3 FP);

	// Calculate object moiton directon and magnitude based on direction in depth angle.
	Vector3 CalculateObjMotionInDepthDir(int objNum);
	
	// Calculate Velocity analog signal
	void CalVelocitySignal(Vector3 velocityDir);
		
	vector<Affine4Matrix> m_rotCameraCoordMatrixVector[2];
	vector<EyeMask> m_eyeMaskVector[2];
};
