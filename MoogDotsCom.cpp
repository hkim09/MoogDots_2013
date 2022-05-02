#include "StdAfx.h"
#include "MoogDotsCom.h"
#include "GLWindow.h"
#include <wx/busyinfo.h>
#include "MoogDots.h"
#include <process.h>    /* _beginthread, _endthread */

// function pointer typdefs
typedef void (APIENTRY *PFNWGLEXTSWAPCONTROLPROC) (int);
typedef int (*PFNWGLEXTGETSWAPINTERVALPROC) (void);

// declare functions
PFNWGLEXTSWAPCONTROLPROC wglSwapIntervalEXT = NULL;
PFNWGLEXTGETSWAPINTERVALPROC wglGetSwapIntervalEXT = NULL;

// Parameter list -- Original declaration can be found in ParameterList.cpp
extern CParameterList g_pList;
extern float CENTER2SCREEN;
extern int SCREEN_WIDTH;
extern int SCREEN_HEIGHT;
extern double PLATFORM_ROT_CENTER_X, PLATFORM_ROT_CENTER_Y, PLATFORM_ROT_CENTER_Z;
extern double CUBE_ROT_CENTER_X, CUBE_ROT_CENTER_Y, CUBE_ROT_CENTER_Z;

#if RECORD_MOVIE
	CGLToMovie g_MovieRecorder(	"Output.avi", 
#if DUAL_MONITORS
								GetSystemMetrics(SM_CXSCREEN),	/*Movie Frame Width*/
								GetSystemMetrics(SM_CYSCREEN),	/*Movie Frame Height*/
#else
								800,	/*Movie Frame Width*/
								870,	/*Movie Frame Height*/
#endif
								24,		/*Bits Per Pixel*/
								mmioFOURCC('C','V','I','D'),	/*Video Codec for Frame Compression*/
								60);		/*Frames Per Second (FPS)*/
#endif

MoogDotsCom::MoogDotsCom(CMainFrame *mainFrame, char *mbcIP, int mbcPort, char *localIP, int localPort, bool useCustomTimer) :
			 CORE_CONSTRUCTOR, m_glWindowExists(false), m_isLibLoaded(false), m_messageConsole(NULL),
		     m_tempoHandle(-1), m_drawRegularFeedback(true),
			/* m_previousLateral(0.0), m_previousSurge(0.0), m_previousHeave(MOTION_BASE_CENTER), */
			 m_previousBitLow(true),
			 stimAnalogOutput(0),stimAnalogMult(1.0), openGLsignal(0.0), motionType(0)
{
	m_mainFrame = mainFrame;

	// Create the OpenGL display window.
#if DUAL_MONITORS
#if FLIP_MONITORS
	m_glWindow = new GLWindow("GL Window", SCREEN_WIDTH, 0, SCREEN_WIDTH, SCREEN_HEIGHT, createFrustum(), createStarField());
#else
	m_glWindow = new GLWindow("GL Window", 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, createFrustum(), createStarField());
#endif
#else
	m_glWindow = new GLWindow("GL Window", 450, 100, 800, 870, createFrustum(), createStarField());
#endif

#if SHOW_GL_WINDOW
	m_glWindow->Show(true);
#endif

//#if CUSTOM_TIMER - Johnny 6/17/2007
	m_doSyncPulse = false;
	m_customTimer = false;
//#endif;

	if(g_pList.GetVectorData("MOOG_CTRL_TIME").at(0)) UseMoogCtrlTimer(true);
	else UseMoogCtrlTimer(false);

	// Initialize the previous position data.
	m_previousPosition.heave = MOTION_BASE_CENTER; m_previousPosition.lateral = 0.0f;
	m_previousPosition.surge = 0.0f; m_previousPosition.roll = 0.0f;
	m_previousPosition.yaw = 0.0f; m_previousPosition.pitch = 0.0f;

	m_glWindowExists = true;
	m_setRotation = false;
	m_continuousMode = false;
	objMotionType = g_pList.GetVectorData("OBJ_MOTION_TYPE");

#if VERBOSE_MODE
	m_verboseMode = true;
#else
	m_verboseMode = false;
#endif

#if LISTEN_MODE
	m_listenMode = true;
#else
	m_listenMode = false;
#endif

	QueryPerformanceFrequency(&m_freq);

	m_delay = 0.0;

	// Set the packet rate.
	SetPacketRate(16.594);
	//SetPacketRate(16.594/2.0);

//#if !CUSTOM_TIMER - Johnny 6-12-07
	// Setup the vsync functions and, by default, turn off vsync.
	InitVSync();
	SetVSyncState(false);
//#endif - Johnny 6-12-07

	// Set the connection type to the default one.
#if DEF_LISTEN_MODE == 1
	m_connectionType = ConnectionType::Tempo;
#elif DEF_LISTEN_MODE == 2
	m_connectionType = ConnectionType::Pipes;
#else
	m_connectionType = ConnectionType::None;
#endif

	m_matlabRDX = NULL;

	m_pipeHandle = INVALID_HANDLE_VALUE;

	m_speedBuffer = new double[SPEED_BUFFER_SIZE];
	m_speedBuffer[0]= 1.0;
	for(int i=1; i<SPEED_BUFFER_SIZE; i++) m_speedBuffer[i]=pow(EXP_BASE, i+1);

#if RECORD_MOVIE
	recordMovie = false;
#endif

}

//We need adjust SPEED_BUFFER_SIZE and EXP_BASE, so using pointer for m_speedBuffer is 
//much easy to change the value. -Johnny 9/9/2010
//double const MoogDotsCom::m_speedBuffer[SPEED_BUFFER_SIZE] =
//{
//	EXP_BASE, pow(EXP_BASE, 2), pow(EXP_BASE, 3), pow(EXP_BASE, 4), pow(EXP_BASE, 5),
//	pow(EXP_BASE, 6), pow(EXP_BASE, 7), pow(EXP_BASE, 8), pow(EXP_BASE, 9), pow(EXP_BASE, 10),
//	pow(EXP_BASE, 11), pow(EXP_BASE, 12), pow(EXP_BASE, 13), pow(EXP_BASE, 14), pow(EXP_BASE, 15),
//	pow(EXP_BASE, 16), pow(EXP_BASE, 17), pow(EXP_BASE, 18), pow(EXP_BASE, 19), pow(EXP_BASE, 20),
//	pow(EXP_BASE, 21), pow(EXP_BASE, 22), pow(EXP_BASE, 23), pow(EXP_BASE, 24), pow(EXP_BASE, 25),
//	pow(EXP_BASE, 26), pow(EXP_BASE, 27), pow(EXP_BASE, 28), pow(EXP_BASE, 29), pow(EXP_BASE, 30)
//};

double const MoogDotsCom::m_speedBuffer2[SPEED_BUFFER_SIZE] =
{
	0.9988, 0.9966, 0.9930, 0.9872, 0.9782, 0.9649, 0.9459, 0.9197,
	0.8852, 0.8415, 0.7882, 0.7257, 0.6554, 0.5792, 0.5000, 0.4208,
	0.3446, 0.2743, 0.2118, 0.1585, 0.1148, 0.0803, 0.0541, 0.0351,
	0.0218, 0.0128, 0.0070, 0.0034, 0.0012, 0.0
};

MoogDotsCom::~MoogDotsCom()
{
	if (m_glWindowExists) { 
		m_glWindow->Destroy();
	}

#if USE_MATLAB_RDX
	if (m_matlabRDX != NULL) {
		delete m_matlabRDX;
	}
#endif

	delete [] m_speedBuffer;
}


//#if !CUSTOM_TIMER - Johnny 6/17/07
void MoogDotsCom::InitVSync()
{
	//get extensions of graphics card
	char* extensions = (char*)glGetString(GL_EXTENSIONS);

	// Is WGL_EXT_swap_control in the string? VSync switch possible?
	if (strstr(extensions,"WGL_EXT_swap_control"))
	{
		//get address's of both functions and save them
		wglSwapIntervalEXT = (PFNWGLEXTSWAPCONTROLPROC)
			wglGetProcAddress("wglSwapIntervalEXT");
		wglGetSwapIntervalEXT = (PFNWGLEXTGETSWAPINTERVALPROC)
			wglGetProcAddress("wglGetSwapIntervalEXT");
	}
}


void MoogDotsCom::SetVSyncState(bool enable)
{
	if (enable) {
       wglSwapIntervalEXT(1);
	}
	else {
       wglSwapIntervalEXT(0);
	}

	m_customTimer = enable;
}


bool MoogDotsCom::VSyncEnabled()
{
	return (wglGetSwapIntervalEXT() > 0);
}
//#endif - Johnny 6-12-07


void MoogDotsCom::ListenMode(bool value)
{
	m_listenMode = value;
}


void MoogDotsCom::ShowGLWindow(bool value)
{
	if (m_glWindowExists) {
		m_glWindow->Show(value);
	}
}


void MoogDotsCom::SetConnectionType(MoogDotsCom::ConnectionType ctype)
{
	m_connectionType = ctype;
}


MoogDotsCom::ConnectionType MoogDotsCom::GetConnectionType()
{
	return m_connectionType;
}


void MoogDotsCom::InitPipes()
{
	m_iocomplete = true;

	// Setup the security descriptor for the pipe to allow access from the network.
	InitializeSecurityDescriptor(&m_pipeSD, SECURITY_DESCRIPTOR_REVISION);
	SetSecurityDescriptorDacl(&m_pipeSD, TRUE, (PACL)NULL, FALSE);
	m_pipeSA.nLength = sizeof(m_pipeSA);
	m_pipeSA.lpSecurityDescriptor = &m_pipeSD;
	m_pipeSA.bInheritHandle = false;

	// Initialize the overlapped data structure.
	m_overlappedEvent.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	m_overlappedEvent.Offset = (DWORD)0;
	m_overlappedEvent.OffsetHigh = (DWORD)0;
	if (m_overlappedEvent.hEvent == NULL) {
		wxMessageDialog d(NULL, "Failed to create overlapped event.  Pipes won't work");
		d.ShowModal();
	}

	// Initialize the pipe.
	m_pipeHandle = CreateNamedPipe("\\\\.\\pipe\\moogpipe", PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED,
							   PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT,
							   (DWORD)2, (DWORD)PIPE_BUFSIZE, (DWORD)PIPE_BUFSIZE, 500,
							   (LPSECURITY_ATTRIBUTES)&m_pipeSA);
	if (m_pipeHandle == INVALID_HANDLE_VALUE) {
		wxMessageDialog d(NULL, "Failed to create pipe.  Pipes won't work");
		d.ShowModal();
	}
}


void MoogDotsCom::ClosePipes()
{
	CloseHandle(m_overlappedEvent.hEvent);
	CloseHandle(m_pipeHandle);
	m_messageConsole->InsertItems(1, &wxString("Closed pipes connection."), 0);
}


void MoogDotsCom::InitTempo()
{
	int errCode;
	vector<wxString> errorStrings;

#if PCI_DIO_24H_PRESENT
	m_PCI_DIO24_Object.DIO_board_num = m_PCI_DIO24_Object.GetBoardNum("pci-dio24/s");
#endif

#if USE_ANALOG_OUT_BOARD
	m_PCI_DIO48H_Object.DIO_board_num = m_PCI_DIO24_Object.GetBoardNum("pci-dda04/16");
#else
	m_PCI_DIO48H_Object.DIO_board_num = m_PCI_DIO24_Object.GetBoardNum("pci-dio48h");
#endif
	
	m_PCI_DIO48H_Object.DIO_base_address = m_PCI_DIO48H_Object.Get8255BaseAddr(m_PCI_DIO48H_Object.DIO_board_num, 1) + 4;

	//RDX is handled either via chip #1 on the pci-dio48h (rig #3) or via
	//an ISA version of the DIO24 board (base address set to 0x300
	//The logic here makes this code portable across different configurations
	if (m_PCI_DIO48H_Object.DIO_board_num == -1) { //board not found
		m_RDX_base_address = 0x300;  //default for the ISA board
	}
	else {
		m_RDX_base_address = m_PCI_DIO48H_Object.DIO_base_address;
	}

#if PCI_DIO_24H_PRESENT
	//Configure the PCI DIO board for digital input on the first port and output on second
	errCode = cbDConfigPort(m_PCI_DIO24_Object.DIO_board_num, FIRSTPORTA, DIGITALIN);
	if (errCode != 0) {
		errorStrings.push_back("Error setting up PCI-DIO24 port A!");
	}
	errCode = cbDConfigPort(m_PCI_DIO24_Object.DIO_board_num, FIRSTPORTB, DIGITALOUT);
	if (errCode != 0) {
		errorStrings.push_back("Error setting up PCI-DIO24 port B!");
	}
#else
	// Configure the 48H board for ouput on the first chip, port B.
	errCode = cbDConfigPort(m_PCI_DIO48H_Object.DIO_board_num, FIRSTPORTB, DIGITALOUT);
	if (errCode != 0) {
		errorStrings.push_back("Error setting up PCI-DIO48H port B!");
	}

	// Configure the 48H board for input on the first chip, port A.
	errCode = cbDConfigPort(m_PCI_DIO48H_Object.DIO_board_num, FIRSTPORTA, DIGITALIN);
	if (errCode != 0) {
		errorStrings.push_back("Error setting up PCI-DIO48H port A!");
	}
#endif

#if USE_ANALOG_OUT_BOARD
	// Zero out PCI-DDA02/16 board.
	cbAOut(m_PCI_DIO48H_Object.DIO_board_num, 0, BIP10VOLTS, DASCALE(0));
	cbAOut(m_PCI_DIO48H_Object.DIO_board_num, 1, BIP10VOLTS, DASCALE(0));
	cbAOut(m_PCI_DIO48H_Object.DIO_board_num, 2, BIP10VOLTS, DASCALE(0));
#endif

	// Open up a Tempo handle.
	OpenTempo();
	
	// Print info out to the message console.
	if (m_messageConsole != NULL) {
		m_messageConsole->Append(wxString::Format("Tempo Handle: %d", m_tempoHandle));
		m_messageConsole->Append("--------------------------------------------------------------------------------");
#if PCI_DIO_24H_PRESENT
		m_messageConsole->Append(wxString::Format("PCI_DIO24 board # = %d", m_PCI_DIO24_Object.DIO_board_num));
#endif
		m_messageConsole->Append(wxString::Format("PCI_DIO48H board # = %d", m_PCI_DIO48H_Object.DIO_board_num));
		m_messageConsole->Append(wxString::Format("PCI_DIO48H base address (chip 1) = 0x%04X",
							     m_PCI_DIO48H_Object.DIO_base_address));
		m_messageConsole->Append(wxString::Format("RDX base address = 0x%04X", m_RDX_base_address));

		// Spit out any errors from setting up the digital ports.
		vector <wxString>::iterator iter;
		for (iter = errorStrings.begin(); iter != errorStrings.end(); iter++) {
			m_messageConsole->Append(*iter);
		}

		m_messageConsole->Append("--------------------------------------------------------------------------------");
	}	
}


void MoogDotsCom::OpenTempo()
{
	m_tempoHandle = dx_open_recv(m_RDX_base_address, m_tempoBuffer, sizeof(m_tempoBuffer));
	m_tempoErr = dx_reset(m_tempoHandle);
}


void MoogDotsCom::CloseTempo()
{
	dx_close(m_tempoHandle);
}


void MoogDotsCom::SetConsolePointer(wxListBox *messageConsole)
{
	m_messageConsole = messageConsole;
}


void MoogDotsCom::SetVerbosity(bool value)
{
	m_verboseMode = value;
}


StarField MoogDotsCom::createStarField()
{
	// Create a StarField structure that describes the GL starfield.
	StarField s;
	s.dimensions = g_pList.GetVectorData("STAR_VOLUME");
	s.density = g_pList.GetVectorData("STAR_DENSITY")[0];
	s.triangle_size = g_pList.GetVectorData("STAR_SIZE");
	//s.drawTarget = g_pList.GetVectorData("TARG_CROSS")[0];
	s.drawFixationPoint = g_pList.GetVectorData("FP_ON")[0];
	s.drawTarget1 = g_pList.GetVectorData("TARG1_ON")[0];
	s.drawTarget2 = g_pList.GetVectorData("TARG2_ON")[0];
	s.drawBackground = g_pList.GetVectorData("BACKGROUND_ON")[0];
	s.targetSize = g_pList.GetVectorData("TARGET_SIZE")[0];
	s.starLeftColor = g_pList.GetVectorData("STAR_LEYE_COLOR");
	s.starRightColor = g_pList.GetVectorData("STAR_REYE_COLOR");
	s.luminance = g_pList.GetVectorData("STAR_LUM_MULT")[0];
	s.lifetime = (int)g_pList.GetVectorData("STAR_LIFETIME")[0];
	s.probability = g_pList.GetVectorData("STAR_MOTION_COHERENCE")[0];
	s.use_lifetime = g_pList.GetVectorData("STAR_LIFETIME_ON")[0];
	s.useCutout = g_pList.GetVectorData("USE_CUTOUT").at(0) ? true : false;
	s.useCutoutTexture = g_pList.GetVectorData("USE_CUTOUT_TEXTURE").at(0) ? true : false;
	s.cutoutRadius = g_pList.GetVectorData("CUTOUT_RADIUS").at(0);
	s.drawMode = g_pList.GetVectorData("DRAW_MODE").at(0);
	s.cutoutTextureLineWidth = g_pList.GetVectorData("CUTOUT_TEXTURE_PARAS").at(0);
	s.cutoutTextureSeparation = g_pList.GetVectorData("CUTOUT_TEXTURE_PARAS").at(1);
	s.cutoutTextureRotAngle = g_pList.GetVectorData("CUTOUT_TEXTURE_PARAS").at(2);


	// Fixation point.
	vector<double> a;
	a.push_back(g_pList.GetVectorData("TARG_XCTR")[0]);
	a.push_back(g_pList.GetVectorData("TARG_YCTR")[0]);
	a.push_back(g_pList.GetVectorData("TARG_ZCTR")[0]);
	s.fixationPointLocation = a;

	// Target 1.
	vector<double> b;
	b.push_back(g_pList.GetVectorData("TARG_XCTR")[1]);
	b.push_back(g_pList.GetVectorData("TARG_YCTR")[1]);
	b.push_back(g_pList.GetVectorData("TARG_ZCTR")[1]);
	s.targ1Location = b;

	// Target  2.
	vector<double> c;
	c.push_back(g_pList.GetVectorData("TARG_XCTR")[2]);
	c.push_back(g_pList.GetVectorData("TARG_YCTR")[2]);
	c.push_back(g_pList.GetVectorData("TARG_ZCTR")[2]);
	s.targ2Location = c;

	// Luminous Multi for fixation point, target 1 and target 2
	vector<double> d;
	d.push_back(g_pList.GetVectorData("TARG_LUM_MULT")[0]);
	d.push_back(g_pList.GetVectorData("TARG_LUM_MULT")[1]);
	d.push_back(g_pList.GetVectorData("TARG_LUM_MULT")[2]);
	s.targLumMult = d;

	s.targXsize = g_pList.GetVectorData("TARG_XSIZ");	// X-dimension of FP & targets // following add by Johnny - 11/6/08
	s.targYsize = g_pList.GetVectorData("TARG_YSIZ");	// Y-dimension of FP & targets
	s.targShape = g_pList.GetVectorData("TARG_SHAPE");	// shape of FP & targets: ELLIPSE or RECTANGLE
	s.targRlum = g_pList.GetVectorData("TARG_RLUM");	// red luminance of targets/FP: 0 -> 1
	s.targGlum = g_pList.GetVectorData("TARG_GLUM");	// green luminance of targets/FP: 0 -> 1
	s.targBlum = g_pList.GetVectorData("TARG_BLUM");	// blue luminance of targets/FP: 0 -> 1

	return s;
}


Frustum MoogDotsCom::createFrustum()
{
	vector<double> eyeOffsets = g_pList.GetVectorData("EYE_OFFSETS");
	vector<double> headCenter = g_pList.GetVectorData("HEAD_CENTER");

	// Create a new Frustum structure that describes the GL space we'll be working with.
	Frustum f;
	f.screenWidth = g_pList.GetVectorData("SCREEN_DIMS")[0];					// Screen width
	f.screenHeight = g_pList.GetVectorData("SCREEN_DIMS")[1];					// Screen height
	f.clipNear = g_pList.GetVectorData("CLIP_PLANES")[0];						// Near clipping plane.
	f.clipFar = g_pList.GetVectorData("CLIP_PLANES")[1];						// Far clipping plane.
	f.eyeSeparation = g_pList.GetVectorData("IO_DIST")[0];						// Distance between eyes.
	f.camera2screenDist = CENTER2SCREEN - eyeOffsets.at(1) - headCenter.at(1);	// Distance from monkey to screen.
	f.worldOffsetX = eyeOffsets.at(0) + headCenter.at(0);						// Horizontal world offset.
	f.worldOffsetZ = eyeOffsets.at(2) + headCenter.at(2);						// Vertical world offset.

#if !DUAL_MONITORS
	f.screenWidth = 22.5;					// Screen width
	f.screenHeight = 24.5;					// Screen height
#endif

	return f;
}


void MoogDotsCom::UpdateGLScene(bool doSwapBuffers)
{
	bool drawTarget = true, drawBackground = true;
	GLPanel *g = m_glWindow->GetGLPanel();

	// Make sure that the glWindow actually has been created before we start
	// messing with it.
	if (m_glWindowExists == false) {
		return;
	}

	// Create a new Frustum and StarField based on the parameter list.
	Frustum f = createFrustum();
	StarField s = createStarField();

	// Grab pointers to the Frustum and StarField.
	Frustum *pf = g->GetFrustum();
	StarField *ps = g->GetStarField();

	// If the new Frustum and StarField are different from the old ones, then
	// we'll update them.
	if (compareFrustums(f, *pf) == false) {
		g->SetFrustum(f);
	}

	// DISPTODEPTH function uses Frustum, so we call it after setup Frustum.
	if(g_pList.GetVectorData("CLIP_PLANES").at(2)==1.0)// use degree
	{ //Notice: clip near and far from eye position.
		f.clipNear = f.camera2screenDist - g->DISPTODEPTH(f.clipNear);
		f.clipFar = f.camera2screenDist - g->DISPTODEPTH(f.clipFar);
		g->SetFrustum(f);
		//m_messageConsole->InsertItems(1, &wxString::Format("clipNear:%f, clipFar:%f",f.clipNear,f.clipFar), 0);
	}

	//if (compareStarFields(s, *ps) == false) {
		g->SetStarField(s);
	//}

	// Motion Parallax: set parameters
	g->SetStimParams();

	// Target size, target on/off, and background on/off are not checked in the
	// compare functions so that we can manipulate them without forcing the
	// whole starfield to be redrawn.
	//ps->drawTarget = g_pList.GetVectorData("TARG_CROSS").at(0);
	ps->drawFixationPoint = g_pList.GetVectorData("FP_ON").at(0);
	ps->drawTarget1 = g_pList.GetVectorData("TARG1_ON").at(0);
	ps->drawTarget2 = g_pList.GetVectorData("TARG2_ON").at(0);
	ps->drawBackground = g_pList.GetVectorData("BACKGROUND_ON").at(0);
	ps->targetSize = g_pList.GetVectorData("TARGET_SIZE").at(0);
	ps->starLeftColor = g_pList.GetVectorData("STAR_LEYE_COLOR");
	ps->starRightColor = g_pList.GetVectorData("STAR_REYE_COLOR");
	ps->luminance = g_pList.GetVectorData("STAR_LUM_MULT").at(0);
	ps->lifetime = (int)g_pList.GetVectorData("STAR_LIFETIME").at(0);
	ps->probability = g_pList.GetVectorData("STAR_MOTION_COHERENCE").at(0);
	ps->use_lifetime = g_pList.GetVectorData("STAR_LIFETIME_ON").at(0);
	ps->useCutout = g_pList.GetVectorData("USE_CUTOUT").at(0) ? true : false;
	ps->useCutoutTexture = g_pList.GetVectorData("USE_CUTOUT_TEXTURE").at(0) ? true : false;
	ps->cutoutRadius = g_pList.GetVectorData("CUTOUT_RADIUS").at(0);
	ps->cutoutTextureLineWidth = g_pList.GetVectorData("CUTOUT_TEXTURE_PARAS").at(0);
	ps->cutoutTextureSeparation = g_pList.GetVectorData("CUTOUT_TEXTURE_PARAS").at(1);
	ps->cutoutTextureRotAngle = g_pList.GetVectorData("CUTOUT_TEXTURE_PARAS").at(2);

	// Create a conversion factor to convert from TEMPO degrees
	// into centimeters for OpenGL.
	double deg2cm = tan(deg2rad(1.0))*(CENTER2SCREEN -
									   g_pList.GetVectorData("HEAD_CENTER").at(1) -
									   g_pList.GetVectorData("EYE_OFFSETS").at(1));

	// Fixation point.
	vector<double> a;
	a.push_back(g_pList.GetVectorData("TARG_XCTR").at(0)*deg2cm);
	a.push_back(g_pList.GetVectorData("TARG_YCTR").at(0)*deg2cm);
	a.push_back(g_pList.GetVectorData("TARG_ZCTR").at(0)*deg2cm);
	ps->fixationPointLocation = a;

	// Target 1.
	vector<double> b;
	b.push_back(g_pList.GetVectorData("TARG_XCTR").at(1)*deg2cm);
	b.push_back(g_pList.GetVectorData("TARG_YCTR").at(1)*deg2cm);
	b.push_back(g_pList.GetVectorData("TARG_ZCTR").at(1)*deg2cm);
	ps->targ1Location = b;

	// Target  2.
	vector<double> c;
	c.push_back(g_pList.GetVectorData("TARG_XCTR").at(2)*deg2cm);
	c.push_back(g_pList.GetVectorData("TARG_YCTR").at(2)*deg2cm);
	c.push_back(g_pList.GetVectorData("TARG_ZCTR").at(2)*deg2cm);
	ps->targ2Location = c;

	// Luminous Multi for fixation point, target 1 and target 2
	vector<double> d;
	d.push_back(g_pList.GetVectorData("TARG_LUM_MULT")[0]);
	d.push_back(g_pList.GetVectorData("TARG_LUM_MULT")[1]);
	d.push_back(g_pList.GetVectorData("TARG_LUM_MULT")[2]);
	ps->targLumMult = d;

	// X-dimension of FP & targets 
	d.clear();
	for(int i=0; i<3; i++) d.push_back(g_pList.GetVectorData("TARG_XSIZ").at(i)*deg2cm);
	ps->targXsize = d;

	// Y-dimension of FP & targets
	d.clear();
	for(int i=0; i<3; i++) d.push_back(g_pList.GetVectorData("TARG_YSIZ").at(i)*deg2cm);
	ps->targYsize = d;

	// find vertex of ellipse shape and store in vector
	/*for(int i=0; i<3; i++)
	{
		int j=0;
		for(int k=0; k<DRAW_TARG_SLICES; k++)
		{
			ps->targVertex[i][j++] = ps->targXsize.at(i)*cos(k*2*PI/DRAW_TARG_SLICES)/2.0;
			ps->targVertex[i][j++] = ps->targYsize.at(i)*sin(k*2*PI/DRAW_TARG_SLICES)/2.0;
			ps->targVertex[i][j++] = 0;
		}
	}*/

	ps->targShape = g_pList.GetVectorData("TARG_SHAPE");// shape of FP & targets: ELLIPSE or RECTANGLE
	ps->targRlum = g_pList.GetVectorData("TARG_RLUM");	// red luminance of targets/FP: 0 -> 1
	ps->targGlum = g_pList.GetVectorData("TARG_GLUM");	// green luminance of targets/FP: 0 -> 1
	ps->targBlum = g_pList.GetVectorData("TARG_BLUM");	// blue luminance of targets/FP: 0 -> 1

	g->SetupParameters();

	// Output signal to CED
	stimAnalogOutput = g_pList.GetVectorData("STIM_ANALOG_OUTPUT").at(0);
	stimAnalogMult = g_pList.GetVectorData("STIM_ANALOG_MULT").at(0);

	XYZ3 objOrigin;
	for(int i=0; i<NUM_OF_DYN_OBJ; i++)
	{
		objOrigin = m_glWindow->GetGLPanel()->glDynObject[i]->GetObjOrigin();
		m_glWindow->GetGLPanel()->SetObjFieldTran(i, objOrigin.x*100.0, objOrigin.y*100.0, objOrigin.z*100.0);
	}

	// set visual syncronisation
	if (g_pList.GetVectorData("VISUAL_SYNC").at(0)){
		SetVSyncState(true);
		UseCustomTimer(true);
	}
	else {
		SetVSyncState(false);
		UseCustomTimer(false);
	}

	if (doSwapBuffers) {
		// Re-Render the scene.
		g->Render();

		// If we're in the main thread then call the GLPanel's SwapBuffers() function
		// because that function references the context made in the main thread.
		// Otherwise, we need to swap the buffers based on the context made in the
		// communications thread.
		if (wxThread::IsMain() == true) {
			g->SwapBuffers();
		}
		else {
			wglMakeCurrent((HDC)g->GetContext()->GetHDC(), m_threadGLContext);
			SwapBuffers((HDC)g->GetContext()->GetHDC());
		}
	}

	if(g_pList.GetVectorData("MOOG_CTRL_TIME").at(0)) UseMoogCtrlTimer(true);
	else UseMoogCtrlTimer(false);
}


bool MoogDotsCom::compareFrustums(Frustum a, Frustum b) const
{
	// Compare every element in the two Frustums.
	bool equalFrustums = a.camera2screenDist == b.camera2screenDist &&
						 a.clipNear == b.clipNear &&
						 a.clipFar == b.clipFar &&
						 a.eyeSeparation == b.eyeSeparation &&
						 a.screenHeight == b.screenHeight &&
						 a.screenWidth == b.screenWidth &&
						 a.worldOffsetX == b.worldOffsetX &&
						 a.worldOffsetZ == b.worldOffsetZ;

	return equalFrustums;
}

bool MoogDotsCom::compareStarFields(StarField a, StarField b) const
{
	bool equalStarFields;

	// Compare every element in the two StarFields.
	equalStarFields = a.density == b.density &&
					  a.dimensions == b.dimensions &&
					  a.triangle_size == b.triangle_size &&
					  a.drawMode == b.drawMode;

	return equalStarFields;
}


void MoogDotsCom::Sync()
{
	// Sync to a SwapBuffers() call.
	SetVSyncState(true);
	wglMakeCurrent((HDC)m_glWindow->GetGLPanel()->GetContext()->GetHDC(), m_threadGLContext);
	SwapBuffers((HDC)m_glWindow->GetGLPanel()->GetContext()->GetHDC());
	SetVSyncState(false);
	Delay(m_delay);
}


void MoogDotsCom::ThreadInit(void)
{
	// Setup the rendering context for the thread.  Every thread has to
	// have its own rendering context.
	if (m_glWindowExists) {
		m_threadGLContext = wglCreateContext((HDC)m_glWindow->GetGLPanel()->GetContext()->GetHDC());

		// Make sure that we got a valid handle.
		if (m_threadGLContext == NULL) {
			wxMessageDialog d(NULL, "ThreadInit: Couldn't create GL Context.", "GL Error");
			d.ShowModal();
		}

		if (wglMakeCurrent((HDC)m_glWindow->GetGLPanel()->GetContext()->GetHDC(), m_threadGLContext) == FALSE) {
			wxMessageDialog d(NULL, "ThreadInit: Couldn't MakeCurrent.", "GL ERROR");
			d.ShowModal();
		}

		// Initialize the GL Session.
		m_glWindow->GetGLPanel()->InitGL();
	}

	m_data.index = 0;
	m_recordOffset = 0;
	m_recordIndex = 0;
	m_grabIndex = 0;

	// If we're using pipes, wait for a connection.
	if (m_connectionType == MoogDotsCom::ConnectionType::Pipes) {
		ConnectNamedPipe(m_pipeHandle, &m_overlappedEvent);

		// Wait for the pipe to get signaled.
		m_messageConsole->InsertItems(1, &wxString("Waiting for client connection..."), 0);

        WaitForSingleObject(m_overlappedEvent.hEvent, INFINITE);
		m_messageConsole->InsertItems(1, &wxString("Connected!"), 0);

		// Check the result.
		DWORD junk;
		if (GetOverlappedResult(m_pipeHandle, &m_overlappedEvent, &junk, FALSE) == 0) {
			wxMessageDialog d(NULL, "GetOverlappedResult failed.");
			d.ShowModal();
		}

		ResetEvent(m_overlappedEvent.hEvent);
	}

#if USE_MATLAB_RDX
	// Create the Matlab RDX communication object if it doesn't exist.
	if (m_matlabRDX == NULL) {
		m_matlabRDX = new CMatlabRDX(m_PCI_DIO48H_Object.DIO_board_num);
	}
	m_matlabRDX->InitClient();
#endif
}


#if USE_MATLAB

void MoogDotsCom::StartMatlab()
{
	m_engine = engOpen(NULL);
}


void MoogDotsCom::CloseMatlab()
{
	engClose(m_engine);
}

void MoogDotsCom::StuffMatlab()
{
	stuffDoubleVector(m_sendStamp, "sendTimes");
	stuffDoubleVector(m_recordedHeave, "rHeave");
	stuffDoubleVector(m_recordedLateral, "rLateral");
	stuffDoubleVector(m_recordedSurge, "rSurge");
	stuffDoubleVector(m_recordedYaw, "rYaw");
	stuffDoubleVector(m_recordedPitch, "rPitch");
	stuffDoubleVector(m_recordedRoll, "rRoll");
	stuffDoubleVector(m_receiveStamp, "receiveTimes");

	// Stuff the command data into Matlab.
	stuffDoubleVector(m_data.X, "dataX");
	stuffDoubleVector(m_data.Y, "dataY");
	stuffDoubleVector(m_data.Z, "dataZ");

	// Stuff the interpolated, predicted data into Matlab.
	stuffDoubleVector(m_interpHeave, "iHeave");
	stuffDoubleVector(m_interpSurge, "iSurge");
	stuffDoubleVector(m_interpLateral, "iLateral");
	stuffDoubleVector(m_interpRotation, "iRotation");

	// Stuff the noise data.
	stuffDoubleVector(m_noise.X, "noiseX");
	stuffDoubleVector(m_noise.Y, "noiseY");
	stuffDoubleVector(m_noise.Z, "noiseZ");
	stuffDoubleVector(m_filteredNoise.X, "fnoiseX");
	stuffDoubleVector(m_filteredNoise.Y, "fnoiseY");
	stuffDoubleVector(m_filteredNoise.Z, "fnoiseZ");

	// Stuff rotation data.
	stuffDoubleVector(m_rotData.X, "yaw");
	stuffDoubleVector(m_rotData.Y, "pitch");
	stuffDoubleVector(m_rotData.Z, "roll");

	stuffDoubleVector(m_fpRotData.X, "fpA");
	stuffDoubleVector(m_fpRotData.Y, "fpE");

	stuffDoubleVector(m_recordedYaw, "ayaw");
	stuffDoubleVector(m_recordedYawVel, "ayawv");

	// Stuff CED Output
	stuffDoubleVector(m_gaussianTrajectoryData, "CEDInputPos");
	stuffDoubleVector(m_interpStimulusData, "CEDTransformPos");
    stuffDoubleVector(m_StimulusData, "CEDOutputVeloc");
#if SWAP_TIMER
	stuffDoubleVector(m_swapStamp, "swapTimes");
#endif

	if (g_pList.GetVectorData("DRAW_MODE").at(0) == 3.0){ // Dots' hollow sphere
		stuffDoubleVector(m_glWindow->GetGLPanel()->dotSizeDistribute, "dotSizeDistribute");
	}

	if((int)m_glWindow->GetGLPanel()->winPointTraj[LEFT_EYE].X.size() > 0 )
	{
		stuffDoubleVector(m_glWindow->GetGLPanel()->winPointTraj[LEFT_EYE].X, "winPointLeftX");
		stuffDoubleVector(m_glWindow->GetGLPanel()->winPointTraj[LEFT_EYE].Y, "winPointLeftY");
		stuffDoubleVector(m_glWindow->GetGLPanel()->winPointTraj[RIGHT_EYE].X, "winPointRightX");
		stuffDoubleVector(m_glWindow->GetGLPanel()->winPointTraj[RIGHT_EYE].Y, "winPointRightY");
	}
}

void MoogDotsCom::stuffDoubleVector(vector<double> data, const char *variable)
{
	int i;
	mxArray *matData;

	// Create an mxArray large enough to hold the contents of the data vector.
	matData = mxCreateDoubleMatrix(1, data.size(), mxREAL);

	// Clear the variable if it exists.
	string s = "clear "; s += variable;
	engEvalString(m_engine, s.c_str());

	// Stuff the mxArray with the vector data.
	for (i = 0; i < (int)data.size(); i++) {
		mxGetPr(matData)[i] = data[i];
	}

	engPutVariable(m_engine, variable, matData);

	mxDestroyArray(matData);
}

void MoogDotsCom::stuffDoubleVector(vector<int> data, const char *variable)
{
	int i;
	mxArray *matData;

	// Create an mxArray large enough to hold the contents of the data vector.
	matData = mxCreateDoubleMatrix(1, data.size(), mxREAL);

	// Clear the variable if it exists.
	string s = "clear "; s += variable;
	engEvalString(m_engine, s.c_str());

	// Stuff the mxArray with the vector data.
	for (i = 0; i < (int)data.size(); i++) {
		mxGetPr(matData)[i] = data[i];
	}

	engPutVariable(m_engine, variable, matData);

	mxDestroyArray(matData);
}

#endif // USE_MATLAB


string MoogDotsCom::checkTempo()
{
	bool command_complete = false;
	char c;
	short nCnt, nErr;
	string command = "";		// Holds the string returned from the buffer.

	// If nothing is in the queue, don't do anything else.
	nCnt = dx_recv(m_tempoHandle);
	if (!nCnt) {
		return command;
	}

	// Until we get an complete command from the buffer, just loop.
	while (command_complete == false)
	{
		while (nCnt)
		{
			// Check for more characters.
			nErr = dx_getchar(m_tempoHandle, &c);
			if (!nErr) {
				break;			//no more chraracters
			}

			// Look to see if we've gotten to the end of the command.
			if ((c == '\n') || (c == '\r'))
			{
				command_complete = 1;
				break;
			}
			else {
				command += c;	// Add the character to the command string.
			}
		}

		if (command_complete == false) {
			nCnt = dx_recv(m_tempoHandle);
		}
	}

	return command;
}


bool MoogDotsCom::CheckForEStop()
{
	unsigned short digIn;			// Stores the read in estop bit.
	bool eStopActivated = false;	// Indicates if the estop sequence was activated.

	// Read the digital bit.
	cbDIn(ESTOP_IN_BOARDNUM, FIRSTPORTA, &digIn);

	// If it's currently high, but previously low, then a stop command has been issued.
	if ((digIn & 2) && m_previousBitLow == true) {
		eStopActivated = true;

		vector<double> stopVal, zeroCode;
		stopVal.push_back(2.0);
		zeroCode.push_back(0.0);
		
		m_previousBitLow = false;

		if (m_verboseMode) {
			m_messageConsole->InsertItems(1, &wxString("***** Fixation Break!"), 0);
		}

		// Turn off a bunch of visual stuff along with the movement stop.
		g_pList.SetVectorData("DO_MOVEMENT", stopVal);
		g_pList.SetVectorData("BACKGROUND_ON", zeroCode);
		g_pList.SetVectorData("FP_ON", zeroCode);
		g_pList.SetVectorData("TARG1_ON", zeroCode);
		g_pList.SetVectorData("TARG2_ON", zeroCode);
		g_pList.SetVectorData("USE_CUTOUT", zeroCode);
		for(int i=1; i<NUM_OF_DYN_OBJ; i++) zeroCode.push_back(0.0);
		g_pList.SetVectorData("OBJ_ENABLE", zeroCode);
		// This copy from Motion Parallax and need check later - Johnny 12/5/08
		//m_glWindow->GetGLPanel()->SetLateral(0.0);  // Make sure cameras are back to zero.
		//m_glWindow->GetGLPanel()->SetHeave(0.0);
		//m_glWindow->GetGLPanel()->SetSurge(0.0);

//#if CUSTOM_TIMER - Johnny 6/17/2007
		// Turn off sync pulse.
		m_doSyncPulse = false;
//#endif

#if USE_ANALOG_OUT_BOARD
		// Zero the analog out board.
		cbAOut(m_PCI_DIO48H_Object.DIO_board_num, 0, BIP10VOLTS, DASCALE(0.0));
		cbAOut(m_PCI_DIO48H_Object.DIO_board_num, 1, BIP10VOLTS, DASCALE(0.0));
		cbAOut(m_PCI_DIO48H_Object.DIO_board_num, 2, BIP10VOLTS, DASCALE(0.0));
#endif
	}

	// Reset the previous bit setting to low if the current bit state
	// is low.
	if ((digIn & 2) == 0) {
		m_previousBitLow = true;
	}

	return eStopActivated;
}


void MoogDotsCom::Control()
{
	string command = "";
	LARGE_INTEGER st, fi;
	double start, finish;
	bool stuffChanged = false, eStop = false;// updateGLScene = true;
	BOOL result;

	// Don't do anything if listen mode isn't enabled or the connection type is set to none.
	if (m_listenMode == false || m_connectionType == MoogDotsCom::ConnectionType::None) {
		return;
	}

#if ESTOP
	// Check to see if the estop bit has been set.  If it has, then CheckForEStop will
	// take care of turning the display off and setting the trajectory to be a buffered stop.
	eStop = stuffChanged = CheckForEStop();
#endif

	if (m_connectionType == MoogDotsCom::ConnectionType::Pipes) {
		// Request to read from the pipe if we already haven't done so.
		if (m_iocomplete == true) {
			result = ReadFile(m_pipeHandle, m_pipeBuff, (DWORD)PIPE_BUFSIZE, NULL, &m_overlappedEvent);
			m_iocomplete = false;

			// Check to see if there was a critical error.
			if (result == 0 && GetLastError() != ERROR_IO_PENDING) {
				wxString e = wxString::Format("*** ReadFile failed with error: %d", GetLastError());
				m_messageConsole->InsertItems(1, &e, 0);
			}
		}
	}

	QueryPerformanceCounter(&st);
	start = static_cast<double>(st.QuadPart) / static_cast<double>(m_freq.QuadPart) * 1000.0;

	try {
		// Loop for a maximum of CONTROL_LOOP_TIME to get as much stuff from the Tempo buffer as possible.
		do {
			// Do Tempo stuff if we have a valid tempo handle and we actually received something on the buffer.
#if USE_MATLAB_RDX
			if (m_matlabRDX->ReadString(1.0, 64, &command) > 0) {
#else
			// Depending on the connection type, we check for the presence of new data
			// in different ways.  
			bool commandReceived = false;
			if (m_connectionType == MoogDotsCom::ConnectionType::Pipes) {
				// Check to see if any new data has arrived.
				CheckPipes();
				commandReceived = !m_commandQueue.empty();

				// If the command queue isn't empty, read the command off the front of
				// the queue and pop it off.
				if (commandReceived) {
					command = m_commandQueue.front();
					m_commandQueue.pop();
				}
			}
			else if (m_connectionType == MoogDotsCom::ConnectionType::Tempo) {
				commandReceived = m_tempoHandle >= 0 && (command = checkTempo()) != "";
			}

			if (commandReceived) {
#endif
				stuffChanged = true;

				// Parses the command received from Tempo or the pipes.
				//updateGLScene = ProcessCommand(command);
				ProcessCommand(command);
			}
			else {
				start = 0.0;
			}

			QueryPerformanceCounter(&fi);
			finish = static_cast<double>(fi.QuadPart) / static_cast<double>(m_freq.QuadPart) * 1000.0;
		} while ((finish - start) < CONTROL_LOOP_TIME);
	}
	catch (exception &e) {
		stuffChanged = false;
		m_messageConsole->InsertItems(1, &wxString("*** Serious screwup detected!"), 0);
		m_messageConsole->InsertItems(1, &wxString(e.what()), 0);
	}

	// Only waste time updating stuff if we actually received valid data from Tempo.
	if (stuffChanged) {
		// Updates basic Moog trajectory movement, including whether or not to move.
		//UpdateMovement();

		//For checking how many time computing starfield.
		if(m_glWindow->GetGLPanel()->GetComputeStarField() &&  m_verboseMode)
			m_messageConsole->InsertItems(1, &wxString("Generate StarField ..."), 0);

		// Updates the GL scene.
		//if(!eStop && updateGLScene) 
		UpdateGLScene(true);
		//Only compute all kinds of star field when get commend DO_MOVEMENT == -1 in function UpdateGLScene(true);
		//After that we need set it to false, so the future commend will not compute star field anymore. -Johnny 4/19/2011
		m_glWindow->GetGLPanel()->SetComputeStarField(false);

		// We need update UpdateGLScene first, 
		// becuase we need new Frustum to setup the Viewport when FP_ROTATE = 2 or 3. -Johnny 10/22/09
		// Updates basic Moog trajectory movement, including whether or not to move.
		UpdateMovement();
	}
} // End void MoogDotsCom::Control()


void MoogDotsCom::CheckPipes()
{
	BOOL result;
	DWORD numBytesRead;
	string command;
	int i = 0, offset = 0;
	bool keepReading = true;

	// Check to see if ReadFile() finished.
	result = WaitForSingleObject(m_overlappedEvent.hEvent, 0);
	switch (result)
	{
		// ReadFile() hasn't finished yet.
	case WAIT_TIMEOUT:
		break;

		// Object is signaled, i.e. ReadFile() completed.
	case WAIT_OBJECT_0:
		// Get the result of the ReadFile() call and reset the signal.
		if (GetOverlappedResult(m_pipeHandle, &m_overlappedEvent, &numBytesRead, FALSE) == 0) {
			wxString es = wxString::Format("GetOverlappedResult failed with error: %d", GetLastError());
			m_messageConsole->InsertItems(1, &es, 0);

			//Automatically disconnect to the pipes when pipes disconnect from Spike2
			wxCommandEvent event;
			m_mainFrame->OnMenuToolsDisconnectPipes(event);
		}
		ResetEvent(m_overlappedEvent.hEvent);
		m_iocomplete = true;

		// Find the newline character and copy over the buffer into the string
		// before that character.
		//while (keepReading) {
		//	if (m_pipeBuff[i] == '\n' || m_pipeBuff[i] == '\r') {
		//		keepReading = false;
		//		i--;
		//	}
		//	i++;
		//}
		//command.assign(m_pipeBuff, i);

		for (i = 0; i < static_cast<int>(numBytesRead); i++) {
			if (m_pipeBuff[i] == '\n' || m_pipeBuff[i] == '\r') {
				command.assign(m_pipeBuff, offset, i-offset);

				// Spike2 uses a '\r\n' to represent a newline, so move the iterator
				// past the 2nd element of the newline.
				i++;

				// The offset of the next command string will be one past the current
				// iterator value.
				offset = i + 1;

				m_commandQueue.push(command);
			}
		}

		break;

	case WAIT_ABANDONED:
		m_messageConsole->InsertItems(1, &wxString("Wait abandoned."), 0);
		break;
	} // End switch (result)
}


void MoogDotsCom::ProcessCommand(string command)
{
	string keyword, param;
	int spaceIndex, tmpIndex, tmpEnd;
	double convertedValue;
	vector<double> commandParams;
	//bool updateGLScene = true;

	// This removes lines in the message console if it is getting too long.
	int numItems = m_messageConsole->GetCount();
	if (numItems >= MAX_CONSOLE_LENGTH) {
		for (int i = 0; i <= numItems-MAX_CONSOLE_LENGTH; i++) {
			m_messageConsole->Delete(m_messageConsole->GetCount()-1);
		}
	}

	// Put the command in the message console.
	if (m_verboseMode) {
		wxString s(command.c_str());
		m_messageConsole->InsertItems(1, &s, 0);
	}

	const char *ass = command.c_str();

	// Grab the keyword from the command.
	spaceIndex = command.find(" ", 0);

	// If we don't get a valid index, then we've likely gotten a command
	// for another program.  In that case, we don't parse the command string
	// and just set the keyword to "invalid" so that we ignore whatever
	// we just received.
	if (spaceIndex != string::npos) {
		keyword = command.substr(0, spaceIndex);

		// Loop and grab the parameters from the command string.
		do {
			tmpIndex = command.find_first_not_of(" ", spaceIndex+1);
			tmpEnd = command.find(" ", tmpIndex);

			// If someone accidentally put a space at the end of the
			// command string, then we want to skip extracting out a
			// parameter value.
			if (tmpIndex != string::npos) {
				if (tmpEnd != string::npos) {
					spaceIndex = tmpEnd;

					// Pull out the substring with the number in it.
					param = command.substr(tmpIndex, tmpEnd - tmpIndex);
				}
				else {
					// Pull out the substring with the number in it.
					param = command.substr(tmpIndex, command.size()-1);
				}

				// Convert the string to a double and stuff it in the vector.
				convertedValue = atof(param.c_str());
				commandParams.push_back(convertedValue);
			}
		} while (tmpEnd != string::npos);
	}
	else {
		keyword = "invalid";
	}

	// Set the parameter data if it's supposed to be in the parameter list.
	if (g_pList.Exists(keyword) == true) {
		if (g_pList.IsVariable(keyword) == false) {
			// Make sure that we don't have a parameter count mismatch.  This could
			// cause unexpected results.
			if (static_cast<int>(commandParams.size()) != g_pList.GetParamSize(keyword)) {
				//if (m_verboseMode) {
					wxString s = wxString::Format("INVALID PARAMETER VALUES FOR %s.", keyword.c_str());
					m_messageConsole->InsertItems(1, &s, 0);
				//}
			}
			else {
				g_pList.SetVectorData(keyword, commandParams);

//#if CUSTOM_TIMER - Johnny 6/17/07
				// If we get the BACKGROUND_ON signal, toggle the sync pulses on or off.
				//if (keyword.compare("BACKGROUND_ON") == 0) {
				if (m_customTimer && keyword.compare("BACKGROUND_ON") == 0) {
					if (commandParams.at(0) == 1.0) {
						m_doSyncPulse = true;
					}
					else {
						m_doSyncPulse = false;
					}
				}
//#endif
			//	//We don't call UpdateGLScene() function for following cases.
			//	if(keyword.compare("STATIC_OBJ_ENABLE") == 0)
			//	{
			//		for(int i=0; i<NUM_OF_STATIC_OBJ; i++)
			//			m_glWindow->GetGLPanel()->glStaticObject[i]->SetObjEnable(commandParams.at(i));
			//		updateGLScene = false; //don't call UpdateGLScene() function
			//	}
			//	else if(keyword.compare("OBJ_ENABLE") == 0)
			//	{
			//		for(int i=0; i<NUM_OF_DYN_OBJ; i++)
			//			m_glWindow->GetGLPanel()->glDynObject[i]->SetObjEnable(commandParams.at(i));
			//		updateGLScene = false; //don't call UpdateGLScene() function
			//	}
			//	else if(keyword.compare("USE_CUTOUT_TEXTURE") == 0)
			//	{
			//		m_glWindow->GetGLPanel()->GetStarField()->useCutoutTexture = commandParams.at(0)? true : false;;
			//		
			//	}
			//	else if(keyword.compare("TARG_LUM_MULT") == 0)
			//	{
			//		vector<double> d;
			//		d.push_back(g_pList.GetVectorData("TARG_LUM_MULT")[0]);
			//		d.push_back(g_pList.GetVectorData("TARG_LUM_MULT")[1]);
			//		d.push_back(g_pList.GetVectorData("TARG_LUM_MULT")[2]);
			//		m_glWindow->GetGLPanel()->GetStarField()->targLumMult = d;
			//	}
			//	else if(keyword.compare("DO_MOVEMENT") == 0 && commandParams.at(0) == 1.0)
			//	{
			//		//The command "DO_MOVEMENT -1" will calculate all stuff such as trajectory.
			//		//The command "DO_MOVEMENT 1" will start the trial, so we don't need call UpdateGLScene() function.
			//		//However, we still need update startField, because startField parameter control a lot of On/Off switches,
			//		//such as, BACKGROUND_ON, USE_CUTOUT_TEXTURE, etc.
			//		//Those command may send right before "DO_MOVEMENT 1".
			//		//- Johnny 3/25/2011
			//		//m_glWindow->GetGLPanel()->SetStarField(createStarField());
			//		updateGLScene = false; //don't call UpdateGLScene() function that will relocate all background starts.
			//	}
				if(keyword.compare("DO_MOVEMENT") == 0 && commandParams.at(0) == -1.0)
					m_glWindow->GetGLPanel()->SetComputeStarField(true);
			} // End if (g_pList.Exists(keyword) == true)
		}
		else {
			g_pList.SetVectorData(keyword, commandParams);
		} // End if (g_pList.IsVariable(keyword) == false)
	}
	else {	// Didn't find keyword in the parameter list.
		if (m_verboseMode) 
		{
			wxString s = wxString::Format("UNKNOWN COMMAND: %s.", command.c_str());
			m_messageConsole->InsertItems(1, &s, 0);
		}
	} // End if (superFreak.empty() == false)

	//return updateGLScene;
}


void MoogDotsCom::UpdateMovement()
{
	vector<double> zeroVector;
	int switchCode = static_cast<int>(g_pList.GetVectorData("DO_MOVEMENT").at(0));
	int i;

	zeroVector.push_back(0.0);

	if (g_pList.GetVectorData("GO_TO_ZERO").at(0) == 0.0) {
		DATA_FRAME startFrame;

		// Grab the movement parameters.
		vector<double> startPoint = g_pList.GetVectorData("M_ORIGIN");
		vector<double> rotStartPoint = g_pList.GetVectorData("ROT_ORIGIN");
		startFrame.lateral = startPoint.at(0); startFrame.surge = startPoint.at(1); startFrame.heave = startPoint.at(2);
		startFrame.yaw = rotStartPoint.at(0); startFrame.pitch = rotStartPoint.at(1); startFrame.roll = rotStartPoint.at(2);
		double elevation = g_pList.GetVectorData("M_ELEVATION").at(0);
		double azimuth = g_pList.GetVectorData("M_AZIMUTH").at(0);
		double magnitude = g_pList.GetVectorData("M_DIST").at(0);
		double duration = g_pList.GetVectorData("M_TIME").at(0);
		double sigma = g_pList.GetVectorData("M_SIGMA").at(0);
		motionType = g_pList.GetVectorData("MOTION_TYPE").at(0);
		objMotionType = g_pList.GetVectorData("OBJ_MOTION_TYPE");

		

		switch (switchCode)
		{
		case 0:	// Do nothing
			break;

		case -1:	// compute trajactory
			m_velocityData.clear();
			// Choose what kind of motion we'll be doing.
			switch (motionType)
			{
			case 0:
				// Calculate the trajectory to move the motion base into start position and along the vector.
				CalculateGaussianMovement(&startFrame, elevation, azimuth, magnitude, duration, sigma, true);

				// Generate the analog output for the velocity if needed.
				if (g_pList.GetVectorData("OUTPUT_VELOCITY").at(0)) {
					CalculateStimulusOutput();
				}
				break;
			case 1:
				CalculateRotationMovement();
				break;
			case 2:
				CalculateSinusoidMovement();
				break;
			case 3:
				Calculate2IntervalMovement();
				break;
			case 4:
				CalculateTiltTranslationMovement();
				break;
			case 5:
				CalculateGaborMovement();
				break;
			case 6:
				CalculateStepVelocityMovement();
				break;
			case 7:// Motion Parallax movement(sin movement)
				// Calculate the trajectory to move the motion base into start position and along the vector.
				CalculateSinMovement();
				break;
			case 8:
				CalculateTrapezoidVelocityMovement();
				break;
			case 9: // Translation plus rotation movement
				CalculateTranPlusRotMovement();
				break;
			case 10: 	// Trapezoid Translation plus rotation movement
				CalculateTranPlusRotMovement(MODE_TRAPEZOID);
				break;
			}

			// Object Motion
			for(i=0; i<NUM_OF_DYN_OBJ; i++)
			{
				switch ((int)objMotionType.at(i))
				{
				// Gaussian translation
				case 0:
					CalculateGaussianObjectMovement(i);
					break;
				// Sin(Motion Parallax) object movement
				case 1:
					CalculateSinObjectMovement(i);
					break;
				// Pseudo depth object movement
				case 2:
					CalculatePseudoDepthObjectMovement(i);
					break;
				// Retinal constant speed object movement
				case 3:
					CalculateConstSpeedObjectMovement(i);
					break;
				// Absolute constant speed object movement
				case 4:
					CalculateConstSpeedObjectMovement(i);
					break;
				case 5: //Trapezoid Velocity
					CalculateTrapezoidVelocityObjectMovement(i);
					break;
				}
			}

			// This keeps the program from calculating the Gaussian movement over and over again.
			g_pList.SetVectorData("DO_MOVEMENT", zeroVector);

			// Output signal to CED
			stimAnalogOutput = g_pList.GetVectorData("STIM_ANALOG_OUTPUT").at(0);
			stimAnalogMult = g_pList.GetVectorData("STIM_ANALOG_MULT").at(0);
			
			//When star feild or object start moving, we will change the rendering context,
			// wglMakeCurrent((HDC)m_moogCom->glPanel->GetContext()->GetHDC(), m_threadGLContext);
			// SwapBuffers((HDC)m_moogCom->glPanel->GetContext()->GetHDC());
			//so, we need redo all the glCallList or texture stuff after wglMakeCurrent().
			m_glWindow->GetGLPanel()->redrawCallList = true;
			m_glWindow->GetGLPanel()->redrawObjTexture = true;

			//Initial the fixation position, camera postion, rotation center, ect and send cbAOut()
			InitRender();

			//ThreadDoCompute(RECEIVE_COMPUTE | COMPUTE);

			break;

		case 1: // Start
			ThreadDoCompute(RECEIVE_COMPUTE | COMPUTE);
			break;

		case 2:	// Stop
			vector<double> x;
			double  ixv = 0.0, iyv = 0.0, izv = 0.0,		// Instananeous velocities (m/frame)
					iyawv = 0.0, ipitchv = 0.0, irollv = 0.0;

			// This will keep the Compute() function from trying to draw predicted data,
			// and use real feedback instead.
			m_recordOffset = SPEED_BUFFER_SIZE;

			// Get the current position of each axis.
			DATA_FRAME currentFrame;
			THREAD_GET_DATA_FRAME(&currentFrame);

			// Calculate the instantaneous velocity for each axis.
			ixv = currentFrame.lateral - m_previousPosition.lateral;
			iyv = currentFrame.surge - m_previousPosition.surge;
			izv = currentFrame.heave - m_previousPosition.heave;
			iyawv = currentFrame.yaw - m_previousPosition.yaw;
			ipitchv = currentFrame.pitch - m_previousPosition.pitch;
			irollv = currentFrame.roll - m_previousPosition.roll;

			// Reset the movement data.
			nmClearMovementData(&m_data);
			nmClearMovementData(&m_rotData);

			// Create buffered movement data.
			for (i = 0; i < SPEED_BUFFER_SIZE; i++) {
				// Translational movement data.
				currentFrame.lateral += ixv * m_speedBuffer[i];
				currentFrame.surge += iyv * m_speedBuffer[i];
				currentFrame.heave += izv * m_speedBuffer[i];
				m_data.X.push_back(currentFrame.lateral);
				m_data.Y.push_back(currentFrame.surge);
				m_data.Z.push_back(currentFrame.heave);

				// Rotational movement data.
				currentFrame.yaw += iyawv * m_speedBuffer[i];
				currentFrame.pitch += ipitchv * m_speedBuffer[i];
				currentFrame.roll += irollv * m_speedBuffer[i];
				m_rotData.X.push_back(currentFrame.yaw);
				m_rotData.Y.push_back(currentFrame.pitch);
				m_rotData.Z.push_back(currentFrame.roll);
			}

			// This keeps the program from calculating the stop movement over and over again.
			g_pList.SetVectorData("DO_MOVEMENT", zeroVector);

			cbAOut(m_PCI_DIO48H_Object.DIO_board_num, 2, BIP10VOLTS, DASCALE(0.0));

			break;
		};
	} // End if (g_pList.GetVectorData("GO_TO_ZERO")[0] == 0.0)
	else {
		/* Replace by FindStartFrame() that include M_ORIGIN and ROT_SATART_OFFSET 
		vector<double> startPoint = g_pList.GetVectorData("M_ORIGIN");
		startFrame.lateral = startPoint.at(0);
		startFrame.surge = startPoint.at(1);
		startFrame.heave = startPoint.at(2);
		startFrame.yaw = 0.0f;
		startFrame.pitch = 0.0f;
		startFrame.roll = 0.0f;
		*/
		//DATA_FRAME startFrame = FindStartFrame();

		// This will move the motion base from its current position to (0, 0, 0).
		//CalculateGaussianMovement(&startFrame, 0.0, 0.0, 0.0, 0.0, 0.0, false);

		GoToOrigin();

		g_pList.SetVectorData("GO_TO_ZERO", zeroVector);
		g_pList.SetVectorData("DO_MOVEMENT", zeroVector);

#if USE_ANALOG_OUT_BOARD
		// Zero out PCI-DDA02/16 board.
		cbAOut(m_PCI_DIO48H_Object.DIO_board_num, 0, BIP10VOLTS, DASCALE(0.0));
		cbAOut(m_PCI_DIO48H_Object.DIO_board_num, 1, BIP10VOLTS, DASCALE(0.0));
		cbAOut(m_PCI_DIO48H_Object.DIO_board_num, 2, BIP10VOLTS, DASCALE(0.0));
#endif

		//ThreadDoCompute(RECEIVE_COMPUTE | COMPUTE);
	}
}

void MoogDotsCom::InitRender()
{
	double azimuth = 0.0, elevation = 0.0;
	// Grab a pointer to the GLPanel.
	GLPanel *glPanel = m_glWindow->GetGLPanel();

	if((int)m_interpLateral.size() > 0)
	{
		glPanel->SetLateral(m_interpLateral.at(0)*100.0);
		glPanel->SetHeave(m_interpHeave.at(0)*100.0 + GL_OFFSET);
		glPanel->SetSurge(m_interpSurge.at(0)*100.0);
	}
	glPanel->SetAllObjRotAngle(0);

	// If we're doing rotation, set the rotation data.
	int rotFP = (int)g_pList.GetVectorData("FP_ROTATE").at(0);
	
	// Make sure we set the azimuth and elevation of the fixation point
	// if need to move the fixation window on the Tempo side.
	//int rotFP = (int)g_pList.GetVectorData("FP_ROTATE").at(0);
	if (rotFP == 1 || rotFP == 2) {
		if((int)m_fpRotData.X.size()>0)
		{
			azimuth = m_fpRotData.X.at(0);
			elevation = m_fpRotData.Y.at(0);
		}
		if((int)m_interpRotation.size()>0)
			m_glWindow->GetGLPanel()->SetRotationAngle(m_interpRotation.front());
	}
	else if (rotFP == 3 && (int)m_rotCameraCoordMatrixVector[LEFT_EYE].size() > 0)
	{
		glPanel->SetRotCameraCoordMatrix(LEFT_EYE, m_rotCameraCoordMatrixVector[LEFT_EYE].at(0));
		glPanel->SetRotCameraCoordMatrix(RIGHT_EYE, m_rotCameraCoordMatrixVector[RIGHT_EYE].at(0));
	}

	if(g_pList.GetVectorData("FP_ROTATE_MASK").at(0) > 0.0 && (int)m_eyeMaskVector[LEFT_EYE].size() > 0)
	{
		if(rotFP == 2 || rotFP == 3 || rotFP == 4 || rotFP == 5 || rotFP == 6) 
		{	// fixed samll mask
			glPanel->SetEyeMask(LEFT_EYE, m_eyeMaskVector[LEFT_EYE].front());	
			glPanel->SetEyeMask(RIGHT_EYE, m_eyeMaskVector[RIGHT_EYE].front());	
		}
	}

	if((int)g_pList.GetVectorData("CTRL_STIM_ON").at(0) == 1)
		glPanel->SetStimOn(m_stimOnData.at(0));


#if USE_ANALOG_OUT_BOARD
	int fixWinMoveType = (int)g_pList.GetVectorData("FIX_WIN_MOVE_TYPE").at(0);
	if (fixWinMoveType == 1) { // rot FP
		cbAOut(m_PCI_DIO48H_Object.DIO_board_num, 0, BIP10VOLTS, DASCALE(azimuth));
		cbAOut(m_PCI_DIO48H_Object.DIO_board_num, 1, BIP10VOLTS, DASCALE(elevation));
	}
	else if (fixWinMoveType == 2) // Motion Parallax
	{
		double glLateralVal = m_interpLateral.at(0)*100.0,
			glHeaveVal = m_interpHeave.at(0)*100.0 + GL_OFFSET;
			//glSurgeVal = m_interpSurge.at(m_grabIndex)*100.0;

		// Convert camera movement values into degrees for Tempo.
		//glLateralVal /= CMPERDEG;
		//glHeaveVal /= CMPERDEG;
		glLateralVal /= glPanel->CmPerDeg();
		glHeaveVal /= glPanel->CmPerDeg();
		// Send camera movement information over to Tempo.  And be sure to flip the (lateral only?) sign!
		// If GLU_LOOK_AT == 1, the camera moves, but the fixation point does not, so we zero out camera movement before sending.  JWN 01/08/05
		cbAOut(m_PCI_DIO48H_Object.DIO_board_num, 0, BIP10VOLTS, DASCALE(-glLateralVal*!g_pList.GetVectorData("GLU_LOOK_AT").at(0)));
		cbAOut(m_PCI_DIO48H_Object.DIO_board_num, 1, BIP10VOLTS, DASCALE(glHeaveVal*!g_pList.GetVectorData("GLU_LOOK_AT").at(0)));
	}
#endif
}

void MoogDotsCom::Compute()
{
	if (m_data.index < static_cast<int>(m_data.X.size())) {
		ThreadGetAxesPositions(&m_previousPosition);

		// Setup the frame to send to the Moog.
		DATA_FRAME moogFrame;
		moogFrame.lateral = static_cast<float>(m_data.X.at(m_data.index));
		moogFrame.surge = static_cast<float>(m_data.Y.at(m_data.index));
		moogFrame.heave = static_cast<float>(m_data.Z.at(m_data.index));
		moogFrame.yaw = static_cast<float>(m_rotData.X.at(m_data.index));
		moogFrame.pitch = static_cast<float>(m_rotData.Y.at(m_data.index));
		moogFrame.roll = static_cast<float>(m_rotData.Z.at(m_data.index));
		SET_DATA_FRAME(&moogFrame);

		// Increment the counter which we use to pull data.
		m_data.index++;

		// Record the last send time's stamp.
#if USE_MATLAB
		m_sendStamp.push_back(ThreadGetSendTime());
#endif

#if !RECORD_MODE
		// Grab the shifted and interpolated data to draw.
		if (m_data.index > m_recordOffset && m_grabIndex < static_cast<int>(m_interpLateral.size())) {
//#if !CUSTOM_TIMER - Johnny 6/17/07
			//if(!m_customTimer) SetFirstPortBcbDOut();
			if(!m_customTimer)
			{
				if((int)g_pList.GetVectorData("CTRL_STIM_ON").at(0) == 1)
				{					
					if(m_stimOnData.at(m_grabIndex)) SetFirstPortBcbDOut();
				}
				else SetFirstPortBcbDOut();
			}
//#endif
			if (m_glWindowExists) {
				double azimuth = 0.0, elevation = 0.0;

				// Grab a pointer to the GLPanel.
				GLPanel *glPanel = m_glWindow->GetGLPanel();
#if DOF_MODE
				//  Set the translation components to the camera.
				int size = m_interpLateral.size();
				// for debug only
				//double a = m_interpLateral[m_grabIndex], b = m_interpHeave[m_grabIndex], c = m_interpSurge[m_grabIndex];

				glPanel->SetLateral(m_interpLateral.at(m_grabIndex)*100.0);
				glPanel->SetHeave(m_interpHeave.at(m_grabIndex)*100.0 + GL_OFFSET);
				glPanel->SetSurge(m_interpSurge.at(m_grabIndex)*100.0);
				glPanel->SetAllObjRotAngle(m_grabIndex);

				if(motionType == 9 || motionType == 10) // translation plus rotation movement
					glPanel->SetRotationCenter(m_glRotCenter.X.at(m_grabIndex), m_glRotCenter.Y.at(m_grabIndex), m_glRotCenter.Z.at(m_grabIndex));

				// Set object star field translation
				// Moog coord system: m_glDynObjData:x-Lateral, y-surge, z-heave
				for(int i=0; i<NUM_OF_DYN_OBJ; i++)
				{
					if (m_grabIndex < int(m_glDynObjData[i].X.size())){ 
						//m_glDynObjData use Moog Coord and SetObjFieldTran in OpenGL coord
						glPanel->SetObjFieldTran(i,
												m_glDynObjData[i].X.at(m_grabIndex)*100.0,
												m_glDynObjData[i].Y.at(m_grabIndex)*100.0,
												m_glDynObjData[i].Z.at(m_grabIndex)*100.0);
					}

					if(glPanel->glDynObject[i]->GetObject().enable == 1.0)
					{
						if(glPanel->glDynObject[i]->GetObject().luminanceFlag == 1.0 && (int)objDynLum[i].size() > m_grabIndex)
							glPanel->glDynObject[i]->SetDynLumRatio(objDynLum[i].at(m_grabIndex));
						else
							glPanel->glDynObject[i]->SetDynLumRatio(1.0);
					}
				}

				// If we're doing rotation, set the rotation data.
				int rotFP = (int)g_pList.GetVectorData("FP_ROTATE").at(0);
				if (m_setRotation == true) {
					//Johnny comment out the val, because it never use. And
					//when the index of m_glRotData (m_grabIndex) increase, it will out of boundary and program crash.
					//double val = m_glRotData.at(m_grabIndex);
					glPanel->SetRotationAngle(m_interpRotation.at(m_grabIndex));

					// Make sure we set the azimuth and elevation of the fixation point
					// if need to move the fixation window on the Tempo side.
					//int rotFP = (int)g_pList.GetVectorData("FP_ROTATE").at(0);
					if (rotFP == 1 || rotFP == 2) {
						azimuth = m_fpRotData.X.at(m_grabIndex);
						elevation = m_fpRotData.Y.at(m_grabIndex);
					}
					else if (rotFP == 3)
					{
						glPanel->SetRotCameraCoordMatrix(LEFT_EYE, m_rotCameraCoordMatrixVector[LEFT_EYE].at(m_grabIndex));
						glPanel->SetRotCameraCoordMatrix(RIGHT_EYE, m_rotCameraCoordMatrixVector[RIGHT_EYE].at(m_grabIndex));
					}

					if(g_pList.GetVectorData("FP_ROTATE_MASK").at(0) > 0.0 && (int)m_eyeMaskVector[LEFT_EYE].size() > 0)
					{
						if(rotFP == 2) 
						{	// moving samll mask
							glPanel->SetEyeMask(LEFT_EYE, m_eyeMaskVector[LEFT_EYE].at(m_grabIndex));	
							glPanel->SetEyeMask(RIGHT_EYE, m_eyeMaskVector[RIGHT_EYE].at(m_grabIndex));	
						}
						if(rotFP == 3 || rotFP == 4 || rotFP == 5) 
						{	// fixed samll mask
							glPanel->SetEyeMask(LEFT_EYE, m_eyeMaskVector[LEFT_EYE].front());	
							glPanel->SetEyeMask(RIGHT_EYE, m_eyeMaskVector[RIGHT_EYE].front());	
						}
					}
				}

				if(g_pList.GetVectorData("FP_ROTATE_MASK").at(0) > 0.0 && (int)m_eyeMaskVector[LEFT_EYE].size() > 0 && rotFP == 6)
				{	//rotFP==6 is only translation but with fixed mask
					glPanel->SetEyeMask(LEFT_EYE, m_eyeMaskVector[LEFT_EYE].front());	
					glPanel->SetEyeMask(RIGHT_EYE, m_eyeMaskVector[RIGHT_EYE].front());	
				}

#if USE_ANALOG_OUT_BOARD
				// If we're not outputtinig the velocity profile, then we need to output
				// the horizontal and vertical position for Tempo to correctly draw the
				// fixation target.
				// FIX_WIN_MOVE_TYPE control the analog output channel 0 and 1.
				int fixWinMoveType = (int)g_pList.GetVectorData("FIX_WIN_MOVE_TYPE").at(0);

				if (fixWinMoveType == 1) { // rot FP
					cbAOut(m_PCI_DIO48H_Object.DIO_board_num, 0, BIP10VOLTS, DASCALE(azimuth));
					cbAOut(m_PCI_DIO48H_Object.DIO_board_num, 1, BIP10VOLTS, DASCALE(elevation));
				}
				else if (fixWinMoveType == 2) // Motion Parallax //if (motionType == 7) 
				{
					double glLateralVal = m_interpLateral.at(m_grabIndex)*100.0,
						glHeaveVal = m_interpHeave.at(m_grabIndex)*100.0 + GL_OFFSET;
						//glSurgeVal = m_interpSurge.at(m_grabIndex)*100.0;

					// Convert camera movement values into degrees for Tempo.
					//glLateralVal /= CMPERDEG;
					//glHeaveVal /= CMPERDEG;
					glLateralVal /= glPanel->CmPerDeg();
					glHeaveVal /= glPanel->CmPerDeg();
					// Send camera movement information over to Tempo.  And be sure to flip the (lateral only?) sign!
					// If GLU_LOOK_AT == 1, the camera moves, but the fixation point does not, so we zero out camera movement before sending.  JWN 01/08/05
					cbAOut(m_PCI_DIO48H_Object.DIO_board_num, 0, BIP10VOLTS, DASCALE(-glLateralVal*!g_pList.GetVectorData("GLU_LOOK_AT").at(0)));
					cbAOut(m_PCI_DIO48H_Object.DIO_board_num, 1, BIP10VOLTS, DASCALE(glHeaveVal*!g_pList.GetVectorData("GLU_LOOK_AT").at(0)));
				}
#endif
#else
				// This section is buggy, will probably have to rework it sometime.
				glPanel->SetLateral((m_interpLateral[m_grabIndex] + m_data.X[m_recordOffset])*100.0);
				glPanel->SetHeave((m_interpHeave[m_grabIndex] + m_data.Z[m_recordOffset])*100.0 + GL_OFFSET);
				glPanel->SetSurge((m_interpSurge[m_grabIndex] + m_data.Y[m_recordOffset])*100.0);
#endif // #if DOF_MODE
				
				if((int)g_pList.GetVectorData("CTRL_STIM_ON").at(0) == 1)
				{					
					glPanel->SetStimOn(m_stimOnData.at(m_grabIndex));					
				}
#if USE_MATLAB				
				if(m_grabIndex == 0)				
				{					
					nmClearMovementData(&glPanel->winPointTraj[LEFT_EYE]);					
					nmClearMovementData(&glPanel->winPointTraj[RIGHT_EYE]);				
				}
#endif				
				// Render the scene.
				glPanel->Render();

//#if !CUSTOM_TIMER - Johnny 6/17/06
				if(!m_customTimer){
					// Swap the buffers.
					wglMakeCurrent((HDC)glPanel->GetContext()->GetHDC(), m_threadGLContext);
#if RECORD_MOVIE
					if(recordMovie) 
					{
						if(FAILED(g_MovieRecorder.RecordFrame()))	/*Record the Rendered Frame into the Movie*/
						{
							::MessageBox(NULL, g_MovieRecorder.MovieFile()->GetLastErrorMessage(), _T("Error"), MB_OK | MB_ICONERROR);
							recordMovie = false;
						}
					}
#endif					
					SwapBuffers((HDC)glPanel->GetContext()->GetHDC());
				}
//#endif
				m_drawRegularFeedback = false;

				// If we only show visual display and Moog base doesn't move,
				// we need send some data back after we did SwapBuffers(),?
				// because it take 16.6ms to swap buffer of screen, but sending signal to Spike2 is quick.
				// rotation sine wave: rotation angle
				int channel = 2;
				double moogSignal = 0.0; // signal for Moog base
				double interpSignal = 0.0; // signal for drawing on screen
				double accelerSignal = 0.0; // Simulate accelerometer signal
				//stimAnalogOutput control the analog output channel 2.
				switch( stimAnalogOutput ) 
				{
					case 0:
						if (g_pList.GetVectorData("OUTPUT_VELOCITY").at(0) != 0.0) {
							OutputStimulusCurve(m_grabIndex);				
						}
					case 1:
						if(m_grabIndex<(int)m_interpLateral.size()){
							moogSignal = moogFrame.lateral*100;
							interpSignal =  m_interpLateral.at(m_grabIndex)*100.0;
							accelerSignal =  m_accelerLateral.at(m_grabIndex)*100.0;
						}
						break;
					case 2:
						if(m_grabIndex<(int)m_interpHeave.size()){
							moogSignal = moogFrame.heave*100 + GL_OFFSET; // shift it back to zero.
							interpSignal =  m_interpHeave.at(m_grabIndex)*100.0 + GL_OFFSET;
							accelerSignal =  m_accelerHeave.at(m_grabIndex)*100.0 + GL_OFFSET;
						}
						break;
					case 3:
						if(m_grabIndex<(int)m_interpSurge.size()){
							moogSignal = moogFrame.surge*100;
							interpSignal =  m_interpSurge.at(m_grabIndex)*100.0;
							accelerSignal =  m_accelerSurge.at(m_grabIndex)*100.0;
						}
						break;
					case 4:
						if(m_grabIndex<(int)m_interpRotation.size()){
							moogSignal = moogFrame.yaw;
							interpSignal =  m_interpRotation.at(m_grabIndex);
							accelerSignal =  m_accelerRotation.at(m_grabIndex);
						}
						break;
					case 5:
						if(m_grabIndex<(int)m_interpRotation.size()){
							moogSignal = moogFrame.pitch;
							interpSignal =  m_interpRotation.at(m_grabIndex);
							accelerSignal =  m_accelerRotation.at(m_grabIndex);
						}
						break;
					case 6:
						if(m_grabIndex<(int)m_interpRotation.size()){
							moogSignal = moogFrame.roll;
							interpSignal =  m_interpRotation.at(m_grabIndex);
							accelerSignal =  m_accelerRotation.at(m_grabIndex);
						}
						break;
					case 7: //velocity signal
						double tmp = (double) m_velocityData.size();
						if(m_grabIndex<(int)m_velocityData.size())
						{
							tmp = DASCALE(m_velocityData.at(m_grabIndex));
							cbAOut(m_PCI_DIO48H_Object.DIO_board_num, channel, BIP10VOLTS, m_velocityData.at(m_grabIndex)*32767.5/10+32767.5);
						}
						break;
				}
				if(!m_customTimer){
					if (motionType == 6) // Step velocity
						cbAOut(m_PCI_DIO48H_Object.DIO_board_num, channel, BIP10VOLTS, DASCALE(stimAnalogMult*angularSpeed));
				}
				else{
					if (motionType == 6) // Step velocity
						openGLsignal = stimAnalogMult*angularSpeed;
					else openGLsignal = stimAnalogMult*accelerSignal;
				}
				//cbAOut(m_PCI_DIO48H_Object.DIO_board_num, channel, BIP10VOLTS, DASCALE(moogSignal));
				//cbAOut(m_PCI_DIO48H_Object.DIO_board_num, channel+2, BIP10VOLTS, DASCALE(stimAnalogMult*interpSignal));
			} // if (m_glWindowExists)

			m_grabIndex++;
		} // if (m_data.index > m_recordOffset && m_grabIndex < (int)m_interpLateral.size())
		else {
			//Motion Parallex - comment out in original MoogDots
			//if (motionType == 7) m_drawRegularFeedback = true;
			//turn off the back ground immediately
			if (motionType == 6 && m_grabIndex > 0){ // Step velocity
				vector<double> tmp;
				tmp.push_back(0.0);
				g_pList.SetVectorData("BACKGROUND_ON", tmp);
				g_pList.SetVectorData("FP_ON", tmp);
				for(int i=0; i<5; i++) tmp.push_back(0.0);
				g_pList.SetVectorData("FP_CROSS", tmp);
				UpdateGLScene(true);
				//g_pList.SetVectorData("SIN_FREQUENCY", tmp);
				openGLsignal = 0;
				cbAOut(m_PCI_DIO48H_Object.DIO_board_num, 0, BIP10VOLTS, DASCALE(0.0));
			}
			cbAOut(m_PCI_DIO48H_Object.DIO_board_num, 2, BIP10VOLTS, DASCALE(0.0));
		} // else if (m_data.index > m_recordOffset && m_grabIndex < (int)m_interpLateral.size())
#endif // #if !RECORD_MODE
	}
	else {
		//turn off the back ground immediately
		if (motionType == 6 && m_grabIndex > 0){ // Step velocity
			vector<double> tmp;
			tmp.push_back(0.0);
			g_pList.SetVectorData("BACKGROUND_ON", tmp);
			g_pList.SetVectorData("FP_ON", tmp);
			for(int i=0; i<5; i++) tmp.push_back(0.0);
			g_pList.SetVectorData("FP_CROSS", tmp);
			UpdateGLScene(true);
			//g_pList.SetVectorData("SIN_FREQUENCY", tmp);
			openGLsignal = 0;
			cbAOut(m_PCI_DIO48H_Object.DIO_board_num, 0, BIP10VOLTS, DASCALE(0.0));
		}

		cbAOut(m_PCI_DIO48H_Object.DIO_board_num, 2, BIP10VOLTS, DASCALE(0.0));
		// Stop telling the motion base to move, but keep on calling the ReceiveCompute() function.
		ThreadDoCompute(RECEIVE_COMPUTE);

#if USE_ANALOG_OUT_BOARD
		// If we're doing the analog CED output, zero out the board when we've finished the movement.
		if (g_pList.GetVectorData("OUTPUT_VELOCITY").at(0)) {
			cbAOut(m_PCI_DIO48H_Object.DIO_board_num, 0, BIP10VOLTS, DASCALE(0));
			cbAOut(m_PCI_DIO48H_Object.DIO_board_num, 1, BIP10VOLTS, DASCALE(0));
		}
#endif

		// Go back to regular drawing mode.
		//Motion Parallex - comment out in original MoogDots
		//if (motionType == 7) m_drawRegularFeedback = true;

#if DEBUG_DEFAULTS
		if (m_verboseMode) m_messageConsole->Append(wxString::Format("Compute finished, index = %d", m_data.index));
#endif
	} // if (m_data.index < (int)m_data.X.size())
} // End void MoogDotsCom::Compute()


void MoogDotsCom::ReceiveCompute()
{
	// Get the latest return frame.
	DATA_FRAME returnFrame;
	returnFrame.heave = ThreadGetReturnedHeave();
	returnFrame.lateral = ThreadGetReturnedLateral();
	returnFrame.surge = ThreadGetReturnedSurge();
	returnFrame.yaw = ThreadGetReturnedYaw();
	returnFrame.pitch = ThreadGetReturnedPitch();
	returnFrame.roll = ThreadGetReturnedRoll();

#if RECORD_MODE
	// If we're actively putting movement data into the command buffer, store the
	// return data.  That is, if we're supposed to.
	if (m_data.index > m_recordOffset+0 && m_recordIndex < static_cast<int>(m_data.X.size())-m_recordOffset+11) {
		// Record the receive time of the return packet.
		m_receiveStamp.push_back(ThreadGetReceiveTime());

		m_recordIndex++;

		// We have to subtract off the startpoint of the Gaussian so that the recorded
		// data will always start from 0.
		m_recordedLateral.push_back((returnFrame.lateral - m_data.X[m_recordOffset])*100.0);
		m_recordedHeave.push_back((returnFrame.heave - m_data.Z[m_recordOffset])*100.0 + HEAVE_OFFSET);
		m_recordedSurge.push_back((returnFrame.surge - m_data.Y[m_recordOffset])*100.0);
		m_recordedYaw.push_back(returnFrame.yaw);
		m_recordedPitch.push_back(returnFrame.pitch);
		m_recordedRoll.push_back(returnFrame.roll);
	}
#else // #if RECORD_MODE
	if (m_drawRegularFeedback) {
		// Set the camera position.
		if (m_glWindowExists) {
			m_glWindow->GetGLPanel()->SetLateral(returnFrame.lateral*100.0);
			m_glWindow->GetGLPanel()->SetSurge(returnFrame.surge*100.0);
			m_glWindow->GetGLPanel()->SetHeave(returnFrame.heave*100.0 + HEAVE_OFFSET);

			m_glWindow->GetGLPanel()->Render();

//#if !CUSTOM_TIMER - Johnny 6/17/07
			if(!m_customTimer){
				// Swap the buffers.
				wglMakeCurrent((HDC)m_glWindow->GetGLPanel()->GetContext()->GetHDC(), m_threadGLContext);
				SwapBuffers((HDC)m_glWindow->GetGLPanel()->GetContext()->GetHDC());
			}
//#endif
		}
	}
#endif // #if RECORD_MODE

#if USE_MATLAB && !RECORD_MODE
	m_receiveStamp.push_back(ThreadGetReceiveTime());
#endif
} // ReceiveCompute()



void MoogDotsCom::CustomTimer()
{
//#if CUSTOM_TIMER - Johnny 6/17/07
	if(!m_customTimer) return;

#if SWAP_TIMER
	LARGE_INTEGER t;

	// Time stamp the SwapBuffers() call.
	QueryPerformanceCounter(&t);
	m_swapStamp.push_back((double)t.QuadPart / (double)m_freq.QuadPart * 1000.0);
#endif

	// Swap the buffers.
	wglMakeCurrent((HDC)m_glWindow->GetGLPanel()->GetContext()->GetHDC(), m_threadGLContext);
    SwapBuffers((HDC)m_glWindow->GetGLPanel()->GetContext()->GetHDC());

	// Send out a sync pulse.
	if (m_data.index < static_cast<int>(m_data.X.size()) && m_doSyncPulse == true) 
		SetFirstPortBcbDOut();
	/*
	if (m_doSyncPulse == true) {
		cbDOut(PULSE_OUT_BOARDNUM, FIRSTPORTB, 1);
		cbDOut(PULSE_OUT_BOARDNUM, FIRSTPORTB, 0);
	}
	*/
	
	cbAOut(m_PCI_DIO48H_Object.DIO_board_num, 0, BIP10VOLTS, DASCALE(openGLsignal));
//#endif
}


string MoogDotsCom::replaceInvalidChars(string s)
{
	int i;

	for (i = 0; i < static_cast<int>(s.length()); i++) {
		switch (s[i]) {
			case '-':
				s[i] = 'n';
				break;
			case '.':
				s[i] = 'd';
				break;
		}
	}

	return s;
}


void MoogDotsCom::CalculateCircleMovement()
{
#if USE_MATLAB
	// Values that are only really used when taking debug and feedback data through Matlab.
#if RECORD_MODE
	m_recordedLateral.clear(); m_recordedHeave.clear(); m_recordedSurge.clear();
	m_recordedYaw.clear(); m_recordedPitch.clear(); m_recordedRoll.clear();
#endif
	m_sendStamp.clear(); m_receiveStamp.clear();
#endif

	// Do no move these initializations.  Their location in the function is very important for
	// threading issues.
	m_grabIndex = 0;
	m_recordIndex = 0;

	m_continuousMode = false;

	// This tells Compute() not to set any rotation info and the GLPanel not to try
	// to do any rotation transformations in Render().
	m_setRotation = false;
	m_glWindow->GetGLPanel()->DoRotation(false);

	// Make sure we don't rotate the fixation point.
	m_glWindow->GetGLPanel()->RotationType(0);

	// Moves the platform to start position.
	DATA_FRAME startFrame;
	startFrame.lateral = -0.1f;
	startFrame.surge = startFrame.yaw = startFrame.pitch = startFrame.roll = 0.0f;
	startFrame.heave = 0.0f;
	MovePlatform(&startFrame);

	m_recordOffset = m_data.X.size();

	nm3DDatum p, r;
	p.x = -0.1; p.y = 0.0; p.z = MOTION_BASE_CENTER;
	r.x = 0.0; r.y = 0.0; r.z = MOTION_BASE_CENTER;
	nmRotatePointAboutPoint(p, r, 0.0, 360.0*40.0, 120.0/60.0, 90.0, 0.0, &m_data, false);

	m_glData.X.clear(); m_glData.Y.clear(); m_glData.Z.clear();

	// Make sure the yaw, pitch, and roll components are filled with zeros for the
	// 2nd part of the movement.  Also, copy over the movement data to the glData structure.
	for (int i = m_recordOffset; i < (int)m_data.X.size(); i++) {
		m_rotData.X.push_back(0.0);
		m_rotData.Y.push_back(0.0);
		m_rotData.Z.push_back(0.0);
		m_glData.X.push_back(m_data.X.at(i));
		m_glData.Y.push_back(m_data.Y.at(i));
		m_glData.Z.push_back(m_data.Z.at(i));
	}

	GeneratePredictedData();
}


void MoogDotsCom::CalculateSinusoidMovement()
{
	// This tells Compute() not to set any rotation info and the GLPanel not to try
	// to do any rotation transformations in Render().
	m_setRotation = false;
	m_glWindow->GetGLPanel()->DoRotation(false);

	// Make sure we don't rotate the fixation point.
	m_glWindow->GetGLPanel()->RotationType(0);

	// Do no move these initializations.  Their location in the function is very important for
	// threading issues.
	m_grabIndex = 0;
	m_recordIndex = 0;

	// Clear the OpenGL data.
	nmClearMovementData(&m_glData);

	double duration,
		   freq = g_pList.GetVectorData("SIN_FREQUENCY").at(0);

	if (g_pList.GetVectorData("SIN_CONTINUOUS").at(0) == 1.0) {
		duration = g_pList.GetVectorData("SIN_DURATION").at(0);
		m_continuousMode = true;
	}
	else {
		duration = 1.0/freq;
		m_continuousMode = false;
	}

	if (g_pList.GetVectorData("SIN_MODE").at(0) == 0.0) {
		// Calculate the sinusoid for the platform.
		nmMovementData tmpTraj;
		double amp = g_pList.GetVectorData("SIN_TRANS_AMPLITUDE").at(0);
		double e = g_pList.GetVectorData("SIN_ELEVATION").at(0);
		double a = g_pList.GetVectorData("SIN_AZIMUTH").at(0);
		double step = 1.0/60.0;
		for (double i = 0.0; i < duration + step; i += step) {
			double val = amp*sin(2.0*PI*freq*i + 90.0*DEG2RAD);
			nm3DDatum cv = nmSpherical2Cartesian(e, a, val, true);
			tmpTraj.X.push_back(cv.x);
			tmpTraj.Y.push_back(cv.y);
#if DOF_MODE
			tmpTraj.Z.push_back(cv.z + MOTION_BASE_CENTER);
#else
			tmpTraj.Z.push_back(cv.z);
#endif
		}

		// Do the sinusoid for OpenGL.
		amp = g_pList.GetVectorData("SIN_TRANS_AMPLITUDE").at(1);
		e = g_pList.GetVectorData("SIN_ELEVATION").at(1);
		a = g_pList.GetVectorData("SIN_AZIMUTH").at(1);
		for (double i = 0.0; i < duration + step; i += step) {
			double val = amp*sin(2.0*PI*freq*i + 90.0*DEG2RAD);
			nm3DDatum cv = nmSpherical2Cartesian(e, a, val, true);
			m_glData.X.push_back(cv.x);
			m_glData.Y.push_back(cv.y);
#if DOF_MODE
			m_glData.Z.push_back(cv.z + MOTION_BASE_CENTER);
#else
			m_glData.Z.push_back(cv.z);
#endif
		}
		GeneratePredictedData();

		// Calculates the trajectory to move the platform to start position.
		DATA_FRAME startFrame;
		startFrame.lateral = tmpTraj.X.at(0); startFrame.surge = tmpTraj.Y.at(0); startFrame.heave = tmpTraj.Z.at(0)-MOTION_BASE_CENTER;
		startFrame.yaw = startFrame.pitch = startFrame.roll = 0.0;
		MovePlatform(&startFrame);

		m_recordOffset = static_cast<int>(m_data.X.size());

		// Add the translational sinusoid to the trajectory.
		for (int i = 0; i < static_cast<int>(tmpTraj.X.size()); i++) {
			m_data.X.push_back(tmpTraj.X.at(i));
			m_data.Y.push_back(tmpTraj.Y.at(i));
			m_data.Z.push_back(tmpTraj.Z.at(i));
			m_rotData.X.push_back(0.0);
			m_rotData.Y.push_back(0.0);
			m_rotData.Z.push_back(0.0);
		}
	} // if (g_pList.GetVectorData("SIN_MODE").at(0) == 0.0)
	else {
		vector<double> platformCenter = g_pList.GetVectorData("PLATFORM_CENTER"),
					   headCenter = g_pList.GetVectorData("HEAD_CENTER"),
					   origin = g_pList.GetVectorData("M_ORIGIN"),
					   rotationOffsets = g_pList.GetVectorData("ROT_CENTER_OFFSETS"),
					   eyeOffsets = g_pList.GetVectorData("EYE_OFFSETS");
		double amp = g_pList.GetVectorData("SIN_ROT_AMPLITUDE").at(0);
		double e = -g_pList.GetVectorData("SIN_ELEVATION").at(0);
		double a = g_pList.GetVectorData("SIN_AZIMUTH").at(0);
		double step = 1.0/60.0;

		
		// Create the rotation profile of the sin.
		vector<double> tmpRotTraj;
		double val;
		for (double i = 0.0; i < duration + step; i += step) {
			if (motionType == 6) // Step Velocity
				val = 0.0;
			else val = amp*sin(2.0*PI*freq*i + 90.0*DEG2RAD);
			tmpRotTraj.push_back(val);
		}

		// Point is the center of the platform, rotPoint is the subject's head.
		nm3DDatum point, rotPoint;
		point.x = platformCenter.at(0); 
		point.y = platformCenter.at(1);
		point.z = platformCenter.at(2);
		rotPoint.x = headCenter.at(0)/100.0 + CUBE_ROT_CENTER_X - PLATFORM_ROT_CENTER_X + origin.at(0) + rotationOffsets.at(0)/100.0;
		rotPoint.y = headCenter.at(1)/100.0 + CUBE_ROT_CENTER_Y - PLATFORM_ROT_CENTER_Y + origin.at(1) + rotationOffsets.at(1)/100.0;
		rotPoint.z = headCenter.at(2)/100.0 + CUBE_ROT_CENTER_Z - PLATFORM_ROT_CENTER_Z - origin.at(2) + rotationOffsets.at(2)/100.0;

		vector<double> tmpTraj;
		nmMovementData tmpData, tmpRotData;
		nmRotatePointAboutPoint(point, rotPoint, e, a, &tmpRotTraj, &tmpData, &tmpRotData, true, true);

		// Calculates the trajectory to move the platform to start position.
		DATA_FRAME startFrame;
		startFrame.lateral = tmpData.X.at(0); startFrame.surge = tmpData.Y.at(0); startFrame.heave = -tmpData.Z.at(0);
		startFrame.yaw = tmpRotData.X.at(0); startFrame.pitch = tmpRotData.Y.at(0); startFrame.roll = -tmpRotData.Z.at(0);
		MovePlatform(&startFrame);

		m_recordOffset = static_cast<int>(m_data.X.size());

		// Add the rotational sinusoid to the trajectory.
		for (int i = 0; i < static_cast<int>(tmpData.X.size()); i++) {
			m_data.X.push_back(tmpData.X.at(i));
			m_data.Y.push_back(tmpData.Y.at(i));
			m_data.Z.push_back(-tmpData.Z.at(i) + MOTION_BASE_CENTER);
			m_rotData.X.push_back(tmpRotData.X.at(i));
			m_rotData.Y.push_back(tmpRotData.Y.at(i));
			m_rotData.Z.push_back(-tmpRotData.Z.at(i));
		}

		/*********** Johnny add for dawning OpenGL *************/

		// This tells Compute() to use the rotation info and the GLPanel to use
		// the rotation transformations in Render().
		m_setRotation = true;
		m_glWindow->GetGLPanel()->DoRotation(true);

		// have problem and not fix yet
		// Choose whether we're rotating the fixation point.
		bool rotFP = g_pList.GetVectorData("FP_ROTATE").at(0) >= 1.0 ? true : false;
		m_glWindow->GetGLPanel()->RotationType(static_cast<int>(g_pList.GetVectorData("FP_ROTATE").at(0)));

		double gl_amp = g_pList.GetVectorData("SIN_ROT_AMPLITUDE").at(1);
		double gl_elevation = -g_pList.GetVectorData("SIN_ELEVATION").at(1);
		double gl_azimuth = g_pList.GetVectorData("SIN_AZIMUTH").at(1);
		double gl_freq = g_pList.GetVectorData("SIN_FREQUENCY").at(1);

		// Create the rotation profile of the sin.
		vector<double> angleTrajectory;
		for (double i = 0.0; i < duration + step; i += step) {
			if (motionType == 6) // Step Velocity
				val = 2.0*PI*gl_freq*i*(180/PI); // degree
			else val = gl_amp*sin(2.0*PI*gl_freq*i + 90.0*DEG2RAD);
			angleTrajectory.push_back(val);
		}

#if USE_MATLAB
		stuffDoubleVector(angleTrajectory, "angleTrajectory");
#endif

		// Calculate the rotation vector describing the axis of rotation.
		m_rotationVector = nmSpherical2Cartesian(gl_elevation, gl_azimuth, 1.0, true);

		// Swap the y and z values of the rotation vector to accomodate OpenGL.  We also have
		// to negate the y value because forward is negative in our OpenGL axes.
		double tmp = -m_rotationVector.y;
		m_rotationVector.y = m_rotationVector.z;
		m_rotationVector.z = tmp;
		m_glWindow->GetGLPanel()->SetRotationVector(m_rotationVector);

		// Calulation of offsets for rotation.
		double xdist = -eyeOffsets.at(0) + rotationOffsets.at(3),
			ydist = -eyeOffsets.at(2) + rotationOffsets.at(5),
			zdist = CENTER2SCREEN - headCenter.at(1) - rotationOffsets.at(4);
		m_glWindow->GetGLPanel()->SetRotationCenter(xdist, ydist, zdist);

		// Fill up the OpenGL trajectories for both the rotation and translation.  Translation
		// is actually just set to the zero position because, for this type of movement, the
		// monkey's head shouldn't be translating.
		m_glRotData.clear();
		nmClearMovementData(&m_glData);
		for (int i = 0; i < static_cast<int>(angleTrajectory.size()); i++) {
			m_glRotData.push_back(-angleTrajectory.at(i)); 
			m_glData.X.push_back(0.0); m_glData.Y.push_back(0.0); m_glData.Z.push_back(MOTION_BASE_CENTER);
		}
		GeneratePredictedRotationData();
		GeneratePredictedData();

		// when we draw max and min amplitude line for calibration,
		// we setup the predicted min and max rotation angle.
		if (g_pList.GetVectorData("CALIB_ROT_ON").at(0) == 1.0){
			double minPredictedAngle = m_interpRotation.at(0);
			double maxPredictedAngle = m_interpRotation.at(0);
			double tmpAngle;
			int size = m_interpRotation.size();
			for(int i=5; i<size; i++){
				tmpAngle = m_interpRotation.at(i);
				if(tmpAngle < minPredictedAngle) minPredictedAngle = tmpAngle;
				if(tmpAngle > maxPredictedAngle) maxPredictedAngle = tmpAngle;
			}
			m_glWindow->GetGLPanel()->minPredictedAngle = minPredictedAngle;
			m_glWindow->GetGLPanel()->maxPredictedAngle = maxPredictedAngle;
		}

		// If we're rotatiting the fixation point, we need to generate a trajectory for it
		// so we can spit out values on the D to A board for Tempo to use.
		if (rotFP == true) {
			// Point that will be rotated.
			point.x = 0.0;
			point.y = CENTER2SCREEN - headCenter.at(1) - eyeOffsets.at(1);
			point.z = 0.0;

			rotPoint.x = xdist; rotPoint.y = point.y - zdist; rotPoint.z = ydist;
			double elevation = e, azimuth = a;
			vector<double> tmpRot;
			for(int i=0; i<(int)m_interpRotation.size(); i++) tmpRot.push_back(-m_interpRotation.at(i));
			nmRotatePointAboutPoint(point, rotPoint, elevation, azimuth, &tmpRot,//&angleTrajectory,
									&m_fpData, &m_fpRotData, true, true);

			for (int i = 0; i < static_cast<int>(m_fpData.X.size()); i++) {
				// Find the total distance traveled from the start to end point.
				double x = m_fpData.X.at(i),
					y = m_fpData.Y.at(i),
					z = m_fpData.Z.at(i);
				double tdist = sqrt(x*x + y*y + z*z) / 2.0;

				// If tdist is 0.0 then set elevation manually, otherwise we'll get a divide by zero
				// error.
				if (tdist != 0.0) {
					elevation = asin(z / (tdist*2.0));
				}
				else {
					elevation = 0.0;
				}

				azimuth = atan2(y, x);

				m_fpRotData.X.at(i) = azimuth*RAD2DEG - 90.0;
				m_fpRotData.Y.at(i) = -elevation*RAD2DEG;
			}
		}

	}
}

void MoogDotsCom::CalculateStepVelocityMovement()
{
	// This tells Compute() not to set any rotation info and the GLPanel not to try
	// to do any rotation transformations in Render().
	m_setRotation = false;
	m_glWindow->GetGLPanel()->DoRotation(false);

	// Make sure we don't rotate the fixation point.
	m_glWindow->GetGLPanel()->RotationType(0);

	// Do no move these initializations.  Their location in the function is very important for
	// threading issues.
	m_grabIndex = 0;
	m_recordIndex = 0;

	// Clear the OpenGL data.
	nmClearMovementData(&m_glData);

	m_continuousMode = true;

	// Get all step velocity movement parameters, it only works in visual.
	angularSpeed = g_pList.GetVectorData("STEP_VELOCITY_PARA").at(0);
	double duration = g_pList.GetVectorData("STEP_VELOCITY_PARA").at(1);
	double gl_azimuth = g_pList.GetVectorData("STEP_VELOCITY_PARA").at(2);
	double gl_elevation = -g_pList.GetVectorData("STEP_VELOCITY_PARA").at(3);

	vector<double> platformCenter = g_pList.GetVectorData("PLATFORM_CENTER"),
					headCenter = g_pList.GetVectorData("HEAD_CENTER"),
					origin = g_pList.GetVectorData("M_ORIGIN"),
					rotationOffsets = g_pList.GetVectorData("ROT_CENTER_OFFSETS"),
					eyeOffsets = g_pList.GetVectorData("EYE_OFFSETS");
	double step = 1.0/60.0;

		
	// Create the rotation profile for doing nothing on platform movement.
	vector<double> tmpRotTraj;
	for (double i = 0.0; i < duration + step; i += step) {
		tmpRotTraj.push_back(0.0);
	}

	// Point is the center of the platform, rotPoint is the subject's head.
	nm3DDatum point, rotPoint;
	point.x = platformCenter.at(0); 
	point.y = platformCenter.at(1);
	point.z = platformCenter.at(2);
		rotPoint.x = headCenter.at(0)/100.0 + CUBE_ROT_CENTER_X - PLATFORM_ROT_CENTER_X + origin.at(0) + rotationOffsets.at(0)/100.0;
		rotPoint.y = headCenter.at(1)/100.0 + CUBE_ROT_CENTER_Y - PLATFORM_ROT_CENTER_Y + origin.at(1) + rotationOffsets.at(1)/100.0;
		rotPoint.z = headCenter.at(2)/100.0 + CUBE_ROT_CENTER_Z - PLATFORM_ROT_CENTER_Z - origin.at(2) + rotationOffsets.at(2)/100.0;

	vector<double> tmpTraj;
	nmMovementData tmpData, tmpRotData;
	nmRotatePointAboutPoint(point, rotPoint, 0.0, 0.0, &tmpRotTraj, &tmpData, &tmpRotData, true, true);

	// Calculates the trajectory to move the platform to start position.
	DATA_FRAME startFrame;
	startFrame.lateral = tmpData.X.at(0); startFrame.surge = tmpData.Y.at(0); startFrame.heave = -tmpData.Z.at(0);
	startFrame.yaw = tmpRotData.X.at(0); startFrame.pitch = tmpRotData.Y.at(0); startFrame.roll = -tmpRotData.Z.at(0);
	MovePlatform(&startFrame);

	m_recordOffset = static_cast<int>(m_data.X.size());

	// Add the rotational sinusoid to the trajectory.
	for (int i = 0; i < static_cast<int>(tmpData.X.size()); i++) {
		m_data.X.push_back(tmpData.X.at(i));
		m_data.Y.push_back(tmpData.Y.at(i));
		m_data.Z.push_back(-tmpData.Z.at(i) + MOTION_BASE_CENTER);
		m_rotData.X.push_back(tmpRotData.X.at(i));
		m_rotData.Y.push_back(tmpRotData.Y.at(i));
		m_rotData.Z.push_back(-tmpRotData.Z.at(i));
	}

	/*********** Johnny add for dawning OpenGL *************/

	// This tells Compute() to use the rotation info and the GLPanel to use
	// the rotation transformations in Render().
	m_setRotation = true;
	m_glWindow->GetGLPanel()->DoRotation(true);

	// Create the rotation profile of the sin.
	vector<double> angleTrajectory;
	for (double i = 0.0; i < duration + step; i += step) {
		angleTrajectory.push_back(angularSpeed*i); //degree
	}

#if USE_MATLAB
	stuffDoubleVector(angleTrajectory, "angleTrajectory");
#endif

	// Calculate the rotation vector describing the axis of rotation.
	m_rotationVector = nmSpherical2Cartesian(gl_elevation, gl_azimuth, 1.0, true);

	// Swap the y and z values of the rotation vector to accomodate OpenGL.  We also have
	// to negate the y value because forward is negative in our OpenGL axes.
	double tmp = -m_rotationVector.y;
	m_rotationVector.y = m_rotationVector.z;
	m_rotationVector.z = tmp;
	m_glWindow->GetGLPanel()->SetRotationVector(m_rotationVector);

	// Calulation of offsets for rotation.
	double xdist = -eyeOffsets.at(0) + rotationOffsets.at(3),
		ydist = -eyeOffsets.at(2) + rotationOffsets.at(5),
		zdist = CENTER2SCREEN - headCenter.at(1) - rotationOffsets.at(4);
	m_glWindow->GetGLPanel()->SetRotationCenter(xdist, ydist, zdist);

	// Fill up the OpenGL trajectories for both the rotation and translation.  Translation
	// is actually just set to the zero position because, for this type of movement, the
	// monkey's head shouldn't be translating.
	m_glRotData.clear();
	nmClearMovementData(&m_glData);
	for (int i = 0; i < static_cast<int>(angleTrajectory.size()); i++) {
		m_glRotData.push_back(-angleTrajectory.at(i)); 
		m_glData.X.push_back(0.0); m_glData.Y.push_back(0.0); m_glData.Z.push_back(MOTION_BASE_CENTER);
	}
	GeneratePredictedRotationData();
	GeneratePredictedData();
}


void MoogDotsCom::CalculateTranPlusRotMovement(int mode)
{
	// Grab the movement parameters.
	double elevation = g_pList.GetVectorData("M_ELEVATION").at(0);
	double azimuth = g_pList.GetVectorData("M_AZIMUTH").at(0);
	double magnitude = g_pList.GetVectorData("M_DIST").at(0);
	double duration = g_pList.GetVectorData("M_TIME").at(0);
	double sigma = g_pList.GetVectorData("M_SIGMA").at(0);

	// Check the duration match for translation and rotation movement.
	// If they do not match, we use translation duration M_TIME.
	double rotDuration = g_pList.GetVectorData("ROT_DURATION").at(0);
	if(rotDuration != duration)
	{
		if (m_verboseMode) m_messageConsole->Append(wxString::Format("Base: ROT_DURATION=%f is diff from M_TIME=%f! Use M_TIME instead.", rotDuration, duration));
		g_pList.SetVectorData("ROT_DURATION", g_pList.GetVectorData("M_TIME"));
	}
	if(g_pList.GetVectorData("ROT_DURATION").at(1) != g_pList.GetVectorData("M_TIME").at(1))
	{
		if (m_verboseMode) m_messageConsole->Append(wxString::Format("GL: ROT_DURATION=%f is diff from M_TIME=%f! Use M_TIME instead.", 
																	g_pList.GetVectorData("ROT_DURATION").at(1), g_pList.GetVectorData("M_TIME").at(1)));
		g_pList.SetVectorData("ROT_DURATION", g_pList.GetVectorData("M_TIME"));
	}

	// This does all the rotational stuff for us.  Then all we have to do
	// is calculate the translational offsets and add them in.  
	//if(mode == MODE_GAUSSIAN) CalculateRotationMovement(mode);
	//else if(mode == MODE_TRAPEZOID) CalculateRotationMovement(MODE_TRAPEZOID);
	CalculateRotationMovement(mode);

	// Make sure we don't rotate the fixation point.
	//m_glWindow->GetGLPanel()->RotationType(0);

	// Parameters for the onset and offset delay.
	double onsetDelay = g_pList.GetVectorData("M_TIME_ONSET_DELAY").at(0),
		   gl_onsetDelay = g_pList.GetVectorData("M_TIME_ONSET_DELAY").at(1),
		   offsetDelay = g_pList.GetVectorData("M_TIME_OFFSET_DELAY").at(0),
		   gl_offsetDelay = g_pList.GetVectorData("M_TIME_OFFSET_DELAY").at(1),
		   step = 1.0/60.0,
		   i = 0;

	// Check the onset and offset delay ratio and reset the duration.
	bool error = false;
	if(onsetDelay < 0.0 || gl_onsetDelay < 0.0 || offsetDelay > 1.0 || gl_offsetDelay > 1.0) error = true;
	if(onsetDelay < 0.0) onsetDelay = 0.0;
	if(gl_onsetDelay < 0.0) gl_onsetDelay = 0.0;
	if(offsetDelay > 1.0) offsetDelay = 1.0;
	if(gl_offsetDelay > 1.0) gl_offsetDelay = 1.0;
	if(onsetDelay >= offsetDelay)
	{
		onsetDelay = 0.0;
		offsetDelay = 1.0;
		error = true;
	}
	if(gl_onsetDelay >= gl_offsetDelay)
	{
		gl_onsetDelay = 0.0;
		gl_offsetDelay = 1.0;
		error = true;
	}
	if (m_verboseMode && error)
		m_messageConsole->Append(wxString::Format("Error input: onset or offset delay!"));
	
	// Find the tranlation offset for the platform.
	double trajDuration = duration*(offsetDelay-onsetDelay);
	nmMovementData translationalData;
	nm3DDatum offset;
	offset.x = 0.0; offset.y = 0.0; offset.z = 0.0;
	if(mode == MODE_GAUSSIAN) 
		nmGen3DVGaussTrajectory(&translationalData, elevation, azimuth, magnitude, trajDuration, 60.0, sigma, offset, true, false);
	else if(mode == MODE_TRAPEZOID)
	{
		vector<double> trapezoidVelocityTraj = FindTrapezoidVelocityTraj(	
								g_pList.GetVectorData("M_DIST").at(0), 
								trajDuration, // sec
								g_pList.GetVectorData("TRAPEZOID_TIME1").at(0)*trajDuration, // sec
								g_pList.GetVectorData("TRAPEZOID_TIME2").at(0)*trajDuration); // sec

		for (int j = 0; j < (int)trapezoidVelocityTraj.size(); j++) {
			//find x, y and z component
			nm3DDatum cv = nmSpherical2Cartesian(elevation, azimuth, trapezoidVelocityTraj.at(j), true);
			translationalData.X.push_back(cv.x);
			translationalData.Y.push_back(cv.y);
			translationalData.Z.push_back(cv.z);
		}
	}

	// Create the whole trajectory for whole duration.
	CreateWholeTraj(&translationalData, duration, onsetDelay);

	// We add translation compenent on rotation trajectory that calculate in the beginning.
	// i starts at 1, because 0 frame is at start point and we don't need add any translation on it.
	int index = 0;
	for (int i = 1; i < static_cast<int>(translationalData.X.size()); i++) 
	{
		// This indexes us into the correct location in the movement data.
		index = i + m_recordOffset;
		m_data.X.at(index) += translationalData.X.at(i);
		m_data.Y.at(index) += translationalData.Y.at(i);
		m_data.Z.at(index) += translationalData.Z.at(i);
	}

	// Find the tranlation offset and rotation center offset for OpenGL drawing.
	nmMovementData glTranslationalData;
	elevation = g_pList.GetVectorData("M_ELEVATION").at(1);
	azimuth = g_pList.GetVectorData("M_AZIMUTH").at(1);
	magnitude = g_pList.GetVectorData("M_DIST").at(1);
	duration = g_pList.GetVectorData("M_TIME").at(1);
	sigma = g_pList.GetVectorData("M_SIGMA").at(1);
	double gl_trajDuration = duration*(gl_offsetDelay-gl_onsetDelay);

	//We shall setup the first element of m_glRotCenter at CalculateRotationMovement();
	//vector<double> //platformCenter = g_pList.GetVectorData("PLATFORM_CENTER"),
	//	           headCenter = g_pList.GetVectorData("HEAD_CENTER"),
	//			   //origin = g_pList.GetVectorData("M_ORIGIN"),
	//			   rotationOffsets = g_pList.GetVectorData("ROT_CENTER_OFFSETS"),
	//			   eyeOffsets = g_pList.GetVectorData("EYE_OFFSETS");
	//double xdist = -eyeOffsets.at(0) + rotationOffsets.at(3),
	//	   ydist = -eyeOffsets.at(2) + rotationOffsets.at(5),
	//	   zdist = CENTER2SCREEN - headCenter.at(1) - rotationOffsets.at(4);
	//
	//nmClearMovementData(&m_glRotCenter);

	if(mode == MODE_GAUSSIAN) 
		nmGen3DVGaussTrajectory(&glTranslationalData, elevation, azimuth, magnitude, gl_trajDuration, 60.0, sigma, offset, true, false);
	else if(mode == MODE_TRAPEZOID)
	{
		vector<double> trapezoidVelocityTraj = FindTrapezoidVelocityTraj(	
								g_pList.GetVectorData("M_DIST").at(1), 
								gl_trajDuration, // sec
								g_pList.GetVectorData("TRAPEZOID_TIME1").at(1)*gl_trajDuration, // sec
								g_pList.GetVectorData("TRAPEZOID_TIME2").at(1)*gl_trajDuration); // sec

		for (int j = 0; j < (int)trapezoidVelocityTraj.size(); j++) {
			//find x, y and z component
			nm3DDatum cv = nmSpherical2Cartesian(elevation, azimuth, trapezoidVelocityTraj.at(j), true);
			glTranslationalData.X.push_back(cv.x);
			glTranslationalData.Y.push_back(cv.y);
			glTranslationalData.Z.push_back(cv.z);
		}
	}

	// Create the whole trajectory for whole duration.
	CreateWholeTraj(&glTranslationalData, duration, gl_onsetDelay);

	// Add gl translation compenent to rotation trajectory that calculat in the beginning.
	for (int i = 1; i < static_cast<int>(m_glRotData.size()); i++) 
	{
		m_glData.X.at(i) += glTranslationalData.X.at(i);
		m_glData.Y.at(i) += glTranslationalData.Y.at(i);
		m_glData.Z.at(i) += glTranslationalData.Z.at(i);
	}

	GeneratePredictedRotationData();
	GeneratePredictedData();
	
	// Calulation rotation center for each frame. 
	// We need do this after call function GeneratePredictedData(), because it modified m_glData to m_interpLateral.
	// In CalculateRotationMovement(), (x,y,z) is constant for GL, so all the different are adding glTranslationalData.
	// Therefore, m_interpLateral.at(0)-m_interpLateral.at(i-1) is the translation different after we call GeneratePredictedData().
	// m_glRotCenter.X.push_back(xdist); m_glRotCenter.Y.push_back(ydist); m_glRotCenter.Z.push_back(zdist); 
	// Starting rotation center at m_glRotCenter has already set up in function CalculateRotationMovement();
	double xdist, ydist, zdist;
	for (int i = 1; i < static_cast<int>(m_interpLateral.size()); i++) 
	{
		xdist = m_glRotCenter.X.at(0) + (m_interpLateral.at(i)-m_interpLateral.at(0))*100;
		ydist = m_glRotCenter.Y.at(0) - (m_interpHeave.at(i)-m_interpHeave.at(0))*100;
		zdist = m_glRotCenter.Z.at(0) - (m_interpSurge.at(i)-m_interpSurge.at(0))*100;
		m_glRotCenter.X.push_back(xdist); m_glRotCenter.Y.push_back(ydist); m_glRotCenter.Z.push_back(zdist); 
	}

	m_glWindow->GetGLPanel()->SetRotationCenter(m_glRotCenter.X.at(m_grabIndex), m_glRotCenter.Y.at(m_grabIndex), m_glRotCenter.Z.at(m_grabIndex));

#if USE_MATLAB
	stuffDoubleVector(m_glRotCenter.X, "m_glRotCenterX");
	stuffDoubleVector(m_glRotCenter.Y, "m_glRotCenterY");
	stuffDoubleVector(m_glRotCenter.Z, "m_glRotCenterZ");
#endif
}

void MoogDotsCom::CalculateTiltTranslationMovement()
{
	nmMovementData translationalData;
	int ttMode = g_pList.GetVectorData("TT_MODE").at(0);

	// If we're doing subtractive tilt/translation, then we need to add 90 degrees
	// to the axis of rotation to get the axis of translation.  If we're doing additive,
	// we need to subtract 90 degrees.
	double offset = 90.0;
	if (ttMode == 1.0 || ttMode == 4.0) {
		offset = -90.0;
	}

	// The axis of translation is 90 degrees away from the axis of rotation.
	double amplitude = g_pList.GetVectorData("ROT_AMPLITUDE").at(0),
		   gl_amplitude = g_pList.GetVectorData("ROT_AMPLITUDE").at(1),
		   azimuth = g_pList.GetVectorData("ROT_AZIMUTH").at(0) + offset,
		   gl_azimuth = g_pList.GetVectorData("ROT_AZIMUTH").at(1) + offset,
		   duration = g_pList.GetVectorData("ROT_DURATION").at(0),
		   gl_duration = g_pList.GetVectorData("ROT_DURATION").at(1),
		   sigma = g_pList.GetVectorData("ROT_SIGMA").at(0),
		   gl_sigma = g_pList.GetVectorData("ROT_SIGMA").at(1),
		   step = 1.0/60.0;

	// This does all the rotational stuff for us.  Then all we have to do
	// is calculate the translational offsets and add them in.  If we want a pure translation
	// movement, we set the amplitude to zero, call the function, then set amplitude back.
	// This effectively sets all our data in the trajectory to zero.
	if (ttMode == 3.0 || ttMode == 4.0) {
		vector<double> zeroVector, tmpVector = g_pList.GetVectorData("ROT_AMPLITUDE");
		zeroVector.push_back(0.0); zeroVector.push_back(gl_amplitude);
		g_pList.SetVectorData("ROT_AMPLITUDE", zeroVector);
		CalculateRotationMovement();
		g_pList.SetVectorData("ROT_AMPLITUDE", tmpVector);
	}
	else {
		CalculateRotationMovement();
	}

	// Make sure we don't rotate the fixation point.
	m_glWindow->GetGLPanel()->RotationType(0);

	// Generates a Gaussian that reflects the rotation amplitude over time.
	vector<double> tmpTraj;
	nmGen1DVGaussTrajectory(&tmpTraj, amplitude, duration, 60.0, sigma, 0.0, false);

	// Create a vector that holds all the values for the acceleration
	// we need to match.
	vector<double> taccel;
	for (int i = 0; i < static_cast<int>(tmpTraj.size()); i++) {
		taccel.push_back(sin(tmpTraj.at(i)*DEG2RAD)*9.82);
	}

	// Now integrate our acceleration vector twice to get our position vector.
	vector<double> v, p;
	double isum;
	nmTrapIntegrate(&taccel, &v, isum, 0, static_cast<int>(taccel.size())-1, step);
	nmTrapIntegrate(&v, &p, isum, 0, static_cast<int>(v.size())-1, step);

	// Now we need to rotate the position data given the specified azimuth.  It is assumed
	// that elevation will always be zero.  We only add the x and y components since z is
	// unused.
	double oX = g_pList.GetVectorData("M_ORIGIN").at(0),
		   oY = g_pList.GetVectorData("M_ORIGIN").at(1);
	for (int i = 0; i < static_cast<int>(p.size()); i++) {
		// This indexes us into the correct location in the movement data.
		int index = i + m_recordOffset;

		nm3DDatum d = nmSpherical2Cartesian(0.0, azimuth, p.at(i), true);

		// If we're doing translation movement only, we disregard previous values in the data so that
		// we only have a pure tilt/translation trajectory without rotation compensation
		// data.  If we're doing rotational movement only, we don't add in the translation
		// compensation for gravity.
		switch (ttMode)
		{
			// Rotation + translation
		case 0:
		case 1:
			m_data.X.at(index) += d.x;
			m_data.Y.at(index) += d.y;
			break;

			// Movement only
		case 3:
		case 4:
			m_data.X.at(index) = d.x + oX;
			m_data.Y.at(index) = d.y + oY;
			break;
		};
	}

	// We need to add a buffered stop to the end of the movement so it doesn't jerk, so
	// we employ the same method as when we want to make an emergency stop.  First, we'll
	// grab the last value of the translational and rotation data because we're going to
	// use it many times.
	int lastVal = static_cast<int>(m_data.X.size()) - 1,
		previousVal = lastVal - 1;
	double eLateral = m_data.X.at(lastVal),
		   eSurge = m_data.Y.at(lastVal),
		   eHeave = m_data.Z.at(lastVal),
		   eYaw = m_rotData.X.at(lastVal),
		   ePitch = m_rotData.Y.at(lastVal),
		   eRoll = m_rotData.Z.at(lastVal);
	
	// Calculate the instantaneous velocity for each axis.
	double ixv = eLateral - m_data.X.at(previousVal),
		   iyv = eSurge - m_data.Y.at(previousVal),
		   izv = eHeave - m_data.Z.at(previousVal),
		   iyawv = eYaw - m_rotData.X.at(previousVal),
		   ipitchv = ePitch - m_rotData.Y.at(previousVal),
		   irollv = eRoll - m_rotData.Z.at(previousVal);

	// Create buffered movement data.
	for (int i = 0; i < SPEED_BUFFER_SIZE; i++) {
		// Translational movement data.
		eLateral += ixv * m_speedBuffer2[i];
		eSurge += iyv * m_speedBuffer2[i];
		eHeave += izv * m_speedBuffer2[i];
		m_data.X.push_back(eLateral);
		m_data.Y.push_back(eSurge);
		m_data.Z.push_back(eHeave);

		// Rotational movement data.
		eYaw += iyawv * m_speedBuffer2[i];
		ePitch += ipitchv * m_speedBuffer2[i];
		eRoll += irollv * m_speedBuffer2[i];
		m_rotData.X.push_back(eYaw);
		m_rotData.Y.push_back(ePitch);
		m_rotData.Z.push_back(eRoll);
	}

#if DEBUG_DEFAULTS && USE_MATLAB
	stuffDoubleVector(tmpTraj, "rpos");
	stuffDoubleVector(taccel, "taccel");
	stuffDoubleVector(v, "tvel");
	stuffDoubleVector(p, "tpos");
#endif
}

void MoogDotsCom::CalculateRotationMovement(int mode)
{
	nmMovementData tmpData, tmpRotData;

	m_continuousMode = false;

	vector<double> platformCenter = g_pList.GetVectorData("PLATFORM_CENTER"),
		           headCenter = g_pList.GetVectorData("HEAD_CENTER"),
				   origin = g_pList.GetVectorData("M_ORIGIN"),
				   rotationOffsets = g_pList.GetVectorData("ROT_CENTER_OFFSETS"),
				   eyeOffsets = g_pList.GetVectorData("EYE_OFFSETS"),
				   rotStartOffset = g_pList.GetVectorData("ROT_START_OFFSET");

	// This tells Compute() to use the rotation info and the GLPanel to use
	// the rotation transformations in Render().
	m_setRotation = true;
	m_glWindow->GetGLPanel()->DoRotation(true);

	// Choose whether we're rotating the fixation point.
	//bool rotFP = g_pList.GetVectorData("FP_ROTATE").at(0) >= 1.0 ? true : false;
	int rotFP = (int)g_pList.GetVectorData("FP_ROTATE").at(0);
	m_glWindow->GetGLPanel()->RotationType(static_cast<int>(g_pList.GetVectorData("FP_ROTATE").at(0)));

#if USE_MATLAB
	// Values that are only really used when taking debug and feedback data through Matlab.
#if RECORD_MODE
	m_recordedLateral.clear(); m_recordedHeave.clear(); m_recordedSurge.clear();
	m_recordedLateral.reserve(5000); m_recordedHeave.reserve(5000); m_recordedSurge.reserve(5000);
	m_recordedYaw.clear(); m_recordedPitch.clear(); m_recordedRoll.clear();
	m_recordedYaw.reserve(5000); m_recordedPitch.reserve(5000); m_recordedRoll.reserve(5000);
#endif
	m_sendStamp.clear(); m_receiveStamp.clear();
#endif

	// Do no move these initializations.  Their location in the function is very important for
	// threading issues.
	m_grabIndex = 0;
	m_recordIndex = 0;

	// Point is the center of the platform, rotPoint is the subject's head + offsets.
	nm3DDatum point, rotPoint;
	point.x = platformCenter.at(0) + origin.at(0); 
	point.y = platformCenter.at(1) + origin.at(1);
	point.z = platformCenter.at(2) - origin.at(2);
	rotPoint.x = headCenter.at(0)/100.0 + CUBE_ROT_CENTER_X - PLATFORM_ROT_CENTER_X + origin.at(0) + rotationOffsets.at(0)/100.0;
	rotPoint.y = headCenter.at(1)/100.0 + CUBE_ROT_CENTER_Y - PLATFORM_ROT_CENTER_Y + origin.at(1) + rotationOffsets.at(1)/100.0;
	rotPoint.z = headCenter.at(2)/100.0 + CUBE_ROT_CENTER_Z - PLATFORM_ROT_CENTER_Z - origin.at(2) + rotationOffsets.at(2)/100.0;

	// Parameters for the rotation.
	double amplitude = g_pList.GetVectorData("ROT_AMPLITUDE").at(0),
		   gl_amplitude = g_pList.GetVectorData("ROT_AMPLITUDE").at(1),
		   duration = g_pList.GetVectorData("ROT_DURATION").at(0),
		   gl_duration = g_pList.GetVectorData("ROT_DURATION").at(1),
		   onsetDelay = g_pList.GetVectorData("ROT_DURATION_ONSET_DELAY").at(0),
		   gl_onsetDelay = g_pList.GetVectorData("ROT_DURATION_ONSET_DELAY").at(1),
		   offsetDelay = g_pList.GetVectorData("ROT_DURATION_OFFSET_DELAY").at(0),
		   gl_offsetDelay = g_pList.GetVectorData("ROT_DURATION_OFFSET_DELAY").at(1),
		   sigma = g_pList.GetVectorData("ROT_SIGMA").at(0),
		   gl_sigma = g_pList.GetVectorData("ROT_SIGMA").at(1),

		   // We negate elevation to be consistent with previous program conventions.
		   elevation = -g_pList.GetVectorData("ROT_ELEVATION").at(0),
		   gl_elevation = -g_pList.GetVectorData("ROT_ELEVATION").at(1),

		   azimuth = g_pList.GetVectorData("ROT_AZIMUTH").at(0),
		   gl_azimuth = g_pList.GetVectorData("ROT_AZIMUTH").at(1),
		   step = 1.0/60.0;

	// Check the onset and offset delay ratio and reset the duration.
	bool error = false;
	if(onsetDelay < 0.0 || gl_onsetDelay < 0.0 || offsetDelay > 1.0 || gl_offsetDelay > 1.0) error = true;
	if(onsetDelay < 0.0) onsetDelay = 0.0;
	if(gl_onsetDelay < 0.0) gl_onsetDelay = 0.0;
	if(offsetDelay > 1.0) offsetDelay = 1.0;
	if(gl_offsetDelay > 1.0) gl_offsetDelay = 1.0;
	if(onsetDelay >= offsetDelay)
	{
		onsetDelay = 0.0;
		offsetDelay = 1.0;
		error = true;
	}
	if(gl_onsetDelay >= gl_offsetDelay)
	{
		gl_onsetDelay = 0.0;
		gl_offsetDelay = 1.0;
		error = true;
	}
	if (m_verboseMode && error)
		m_messageConsole->Append(wxString::Format("Error input: onset or offset delay!"));
	double trajDuration = duration*(offsetDelay-onsetDelay);
	double gl_trajDuration = gl_duration*(gl_offsetDelay-gl_onsetDelay);

	// Generate the rotation amplitude with a Gaussian velocity profile.
	vector<double> tmpTraj;
	if(mode == MODE_GAUSSIAN)
		nmGen1DVGaussTrajectory(&tmpTraj, amplitude, trajDuration, 60.0, sigma, 0.0, false);
	else if(mode == MODE_TRAPEZOID)
		tmpTraj = FindTrapezoidVelocityTraj(amplitude, trajDuration, 
						g_pList.GetVectorData("TRAPEZOID_ROT_T1").at(0)*trajDuration, // sec
						g_pList.GetVectorData("TRAPEZOID_ROT_T2").at(0)*trajDuration); // sec
	for(int i=0; i<(int)tmpTraj.size(); i++) tmpTraj.at(i) += rotStartOffset.at(0); // add offset on rotation trajectory.
	nmRotatePointAboutPoint(point, rotPoint, elevation, azimuth, &tmpTraj,
							&tmpData, &tmpRotData, true, true);
	// Create whole trajectory for whole duration
	CreateWholeTraj(&tmpData, duration, onsetDelay);
	CreateWholeTraj(&tmpRotData, duration, onsetDelay);

	// Create the Gaussian rotation trajectory for the OpenGL side of things.
	vector<double> angleTrajectory;
	if(mode == MODE_GAUSSIAN)
		nmGen1DVGaussTrajectory(&angleTrajectory, gl_amplitude, gl_trajDuration, 60.0, gl_sigma, 0.0, false);
	else if(mode == MODE_TRAPEZOID)
		angleTrajectory = FindTrapezoidVelocityTraj(gl_amplitude, gl_trajDuration, 
						g_pList.GetVectorData("TRAPEZOID_ROT_T1").at(1)*gl_trajDuration, // sec
						g_pList.GetVectorData("TRAPEZOID_ROT_T2").at(1)*gl_trajDuration); // sec
	for(int i=0; i<(int)angleTrajectory.size(); i++) angleTrajectory.at(i) += rotStartOffset.at(1); // add offset on rotation trajectory.
	// Create whole trajectory for whole duration
	CreateWholeTraj(&angleTrajectory, gl_duration, gl_onsetDelay);

	// Flip the sign of the roll because the Moog needs to the roll to be opposite of what
	// the preceding function generates.  We also flip the sign of the heave because the
	// equations assume positive is up, whereas the Moog thinks negative is up.
	for (int i = 0; i < static_cast<int>(tmpRotData.X.size()); i++) {
		// The rexroth system uses radians instead of degrees, so we must convert the
		// output of the rotation calculation.
		tmpRotData.Z.at(i) *= -1.0;			// Roll
		tmpData.Z.at(i) *= -1.0;			// Heave
	}

	// Calculate the rotation vector describing the axis of rotation.
	m_rotationVector = nmSpherical2Cartesian(gl_elevation, gl_azimuth, 1.0, true);

	// Swap the y and z values of the rotation vector to accomodate OpenGL.  We also have
	// to negate the y value because forward is negative in our OpenGL axes.
	// Johnny add following command on 2/10/2010
	// nmSpherical2Cartesian() function follows Moog coord system, but not in Z-axis, 
	// because it needs follow right hand rule, when you use sin and cos to find the vector.
	// Thus, nmSpherical2Cartesian() find the vector in coord system(+x->right, +y->forward, +z->upward) 
	// Then opengl +x = moog +x, opengl +y = moog +z, opengl +z = moog -y.
	double tmp = -m_rotationVector.y;
	m_rotationVector.y = m_rotationVector.z;
	m_rotationVector.z = tmp;
	m_glWindow->GetGLPanel()->SetRotationVector(m_rotationVector);
	
	// Calulation of offsets for rotation.
	//double xdist = -eyeOffsets.at(0) + rotationOffsets.at(3),
	//	   ydist = -eyeOffsets.at(2) + rotationOffsets.at(5),
	//	   zdist = CENTER2SCREEN - headCenter.at(1) - rotationOffsets.at(4);
	//m_glWindow->GetGLPanel()->SetRotationCenter(xdist, ydist, zdist);
	//==================
	//Above original code didn't include M_ORIGIN on the rotation center.
	//It still works fine, because inside the huge starfield, what monkey see, is no different when it seats inside the cube,
	//even if physical location of cube is not at center. 
	//Therefore, we always set translation trajectory of Opengl at(0, 0, MOTION_BASE_CENTER) for rotation motion.
	//e.g. m_glData.X.push_back(0.0); m_glData.Y.push_back(0.0); m_glData.Z.push_back(MOTION_BASE_CENTER);
	//==================
	//However, in Motion Parallex will have problem, because it relies on the OpenGL translation trajectory to move camera location.
	//We care about that because tran plus rot motion always find the rotation motion first and adding the translation motion on it.
	//If rotation motion always sets the center at (0, 0, MOTION_BASE_CENTER) for OpenGL, 
	//then the translation motion will always start at this center.
	//For example, the translation motion is from -5 to +5 on x-axis. For tran plus rot motion, 
	//the translation OpenGL trajectory will from 0 to 10, because the rotation motion set the start position at 0.
	//However, Motion Parallex need the start position of camera at -5, so that we will see the position of FP is +5 on screen. 
	//==================
	//Now, we should calculate the rotation center including M_ORIGIN, so that the tran plus rot motion can use on any motion even Motion Parallex.
	//Here, we shall modify the calculate of rotation center and put the OpenGL translation trajectory at rotation center too.
	//==================
	// Calulation of offsets for rotation(with M_ORIGIN). 
	// M_ORIGIN uses Moog coord system and unit is meter.
	// The rotation center uses OpenGL coord system and unit is cm. - Johnny 2/10/2010
	//==================
	// Now, we add a rotation center flag to control the center at head center or cyclopean eye. - Johnny 2/18/2011
	double xdist, ydist, zdist;
	if(g_pList.GetVectorData("GL_ROT_CENTER_FLAG").at(0) == 0) // head center
	{
		xdist = -eyeOffsets.at(0) + rotationOffsets.at(3) + origin.at(0)*100,
		ydist = -eyeOffsets.at(2) + rotationOffsets.at(5) - origin.at(2)*100,
		zdist = CENTER2SCREEN - headCenter.at(1) - rotationOffsets.at(4) - origin.at(1)*100;
	}
	else if(g_pList.GetVectorData("GL_ROT_CENTER_FLAG").at(0) == 1) // cyclopean eye
	{
		xdist = rotationOffsets.at(3) + origin.at(0)*100,
		ydist = rotationOffsets.at(5) - origin.at(2)*100,
		zdist = CENTER2SCREEN - eyeOffsets.at(1) - headCenter.at(1) - rotationOffsets.at(4) - origin.at(1)*100;
	}
	m_glWindow->GetGLPanel()->SetRotationCenter(xdist, ydist, zdist);

	//Setup the first element of m_glRotCenter for tran plus rot motion.
	nmClearMovementData(&m_glRotCenter);
	m_glRotCenter.X.push_back(xdist); m_glRotCenter.Y.push_back(ydist); m_glRotCenter.Z.push_back(zdist); 

	// Fill up the OpenGL trajectories for both the rotation and translation.  Translation
	// is actually just set to the zero position because, for this type of movement, the
	// monkey's head shouldn't be translating.
	// After we apply M_ORIGIN on rotation center, translation need to set at M_ORIGIN. - Johnny 2/10/2010
	// Notice: m_glData uses Moog coord system and unit is meter that is same as M_ORIGIN coord system and unit.
	m_glRotData.clear();
	nmClearMovementData(&m_glData);
	for (int i = 0; i < static_cast<int>(angleTrajectory.size()); i++) {
		m_glRotData.push_back(-angleTrajectory.at(i));
		//m_glData.X.push_back(0.0); m_glData.Y.push_back(0.0); m_glData.Z.push_back(MOTION_BASE_CENTER);
		m_glData.X.push_back(origin.at(0)); 
		m_glData.Y.push_back(origin.at(1)); 
		m_glData.Z.push_back(origin.at(2)+MOTION_BASE_CENTER);
	}
	GeneratePredictedRotationData();
	GeneratePredictedData();

	// We need initalize camera position OpenGL after calculate trajectory.
	// If not, FP will start at wrong position for pursue case.
	m_glWindow->GetGLPanel()->SetLateral(m_interpLateral.front()*100.0);
	m_glWindow->GetGLPanel()->SetHeave(m_interpHeave.front()*100.0 + GL_OFFSET);
	m_glWindow->GetGLPanel()->SetSurge(m_interpSurge.front()*100.0);

	Point3 rotCenter;
	rotCenter.set(xdist, ydist, zdist);
	Vector3 rotVector;
	rotVector.set(m_rotationVector.x, m_rotationVector.y, m_rotationVector.z);
	// In GLvoid GLPanel::Render()
	// glTranslatef(m_starfield.fixationPointLocation[0] + m_Lateral,
	//		m_starfield.fixationPointLocation[1] - m_Heave,
	//		m_starfield.fixationPointLocation[2] - m_Surge);
	// DrawTargetObject(DRAW_FP);
	Point3 FP(m_interpLateral.front()*100.0, -(m_interpHeave.front()*100.0 + GL_OFFSET), -m_interpSurge.front()*100.0);

	// If we're rotatiting the fixation point, we need to generate a trajectory for it
	// so we can spit out values on the D to A board for Tempo to use.
	if (rotFP == 1 || rotFP ==2) { //rotFP==1:rotate FP and Background; rotFP==2:rotate FP but not background - Johnny 10/19/09
		// Point that will be rotated.
		point.x = 0.0;
		point.y = CENTER2SCREEN - headCenter.at(1) - eyeOffsets.at(1);
		point.z = 0.0;

		//rotPoint.x = xdist; rotPoint.y = point.y - zdist; rotPoint.z = ydist;
		//Above is original code from Chris. Now, it will not work, because we include M_ORIGIN in opengl, 
		//so rotation center(xdist, ydist, zdist) is different when M_ORIGIN is not (0,0,0) in OpenGL coord-system.
		//However, FP always at screen and monkey head is not moving, so we don't need include M_ORIGIN in here.
		//Therefore, we restore the rotPoint that without M_ORIGIN effect. -Johnny 4/5/2010
		rotPoint.x = xdist - origin.at(0)*100;
		rotPoint.y = point.y - (zdist + origin.at(1)*100); //It will automatically adjust in either head center or cyclopean eye.
		rotPoint.z = ydist + origin.at(2)*100;

		//The FP signal, that send to Tempo, will faster than FP drawing on screen due to the frame delay.
		//This was found by Jing. Now, we set one frame delay for the FP signal. - Johnny 11/2/2011
		vector<double> tmpRot;
		tmpRot.push_back(-m_interpRotation.at(0));
		for(int i=0; i<(int)m_interpRotation.size(); i++) tmpRot.push_back(-m_interpRotation.at(i));
		nmRotatePointAboutPoint(point, rotPoint, elevation, azimuth, &tmpRot, //&angleTrajectory,
								&m_fpData, &m_fpRotData, true, true);

		for (int i = 0; i < static_cast<int>(m_fpData.X.size()); i++) {
			// Find the total distance traveled from the start to end point.
			double x = m_fpData.X.at(i),
				   y = m_fpData.Y.at(i),
				   z = m_fpData.Z.at(i);
			double tdist = sqrt(x*x + y*y + z*z) / 2.0;

			// If tdist is 0.0 then set elevation manually, otherwise we'll get a divide by zero
			// error.
			if (tdist != 0.0) {
				elevation = asin(z / (tdist*2.0));
			}
			else {
				elevation = 0.0;
			}

			azimuth = atan2(y, x);

			m_fpRotData.X.at(i) = azimuth*RAD2DEG - 90.0;
			m_fpRotData.Y.at(i) = -elevation*RAD2DEG;
		}


		//m_glWindow->GetGLPanel()->SetRotationAngle(m_interpRotation.front());

		//cbAOut(m_PCI_DIO48H_Object.DIO_board_num, 0, BIP10VOLTS, DASCALE(m_fpRotData.X.front()));
		//cbAOut(m_PCI_DIO48H_Object.DIO_board_num, 1, BIP10VOLTS, DASCALE(m_fpRotData.Y.front()));
	}
	else if(rotFP==3) 
	{	// calculate rotation angles for each eye(camera) to simulation pursue case(only rotating FP)
		CalEyeRotation(LEFT_EYE, rotCenter, rotVector, m_interpRotation, FP);
		CalEyeRotation(RIGHT_EYE, rotCenter, rotVector, m_interpRotation, FP);
		m_glWindow->GetGLPanel()->DoRotation(false);
	}
	else if(rotFP==4)
	{	
		// calculate fake rotation that moves a huge object simulation of FP rotation.
		CalFakeRotation(rotCenter, rotVector, m_interpRotation, FP);
		m_setRotation = false;	//stop sending rotation coord to Tempo
		m_glWindow->GetGLPanel()->DoRotation(false); //stop rotation in OpenGL
		objMotionType.at(0) = -1;	//stop recalculate dynamic object 0 trajectory.
	}

	if(rotFP==2 || rotFP==3 || rotFP==4 || rotFP==5 || rotFP==6)
	{	// find masks and make a small moving window for pursue(rotFP==2)
		// and stationary small window for simulation case(rotFP==3)
		CalEyeMask(LEFT_EYE, rotCenter, rotVector, m_interpRotation, FP);
		CalEyeMask(RIGHT_EYE, rotCenter, rotVector, m_interpRotation, FP);
		if(rotFP==6)
		{
			m_setRotation = false;	//stop sending rotation coord to Tempo
			m_glWindow->GetGLPanel()->DoRotation(false); //stop rotation in OpenGL
		}
	}

#if DEBUG_DEFAULTS && USE_MATLAB
	stuffDoubleVector(tmpData.X, "tx");
	stuffDoubleVector(tmpData.Y, "ty");
	stuffDoubleVector(tmpData.Z, "tz");
	stuffDoubleVector(tmpRotData.X, "trx");
	stuffDoubleVector(tmpRotData.Y, "tr_y");
	stuffDoubleVector(tmpRotData.Z, "trz");
	stuffDoubleVector(m_glRotData, "rotData");
	stuffDoubleVector(m_interpRotation, "irotData");
	stuffDoubleVector(m_fpData.X, "fpX");
	stuffDoubleVector(m_fpData.Y, "fpY");
	stuffDoubleVector(m_fpData.Z, "fpZ");
#endif

	// Move the platform into starting position.
	DATA_FRAME startFrame;
	startFrame.lateral = tmpData.X.at(0);
	startFrame.surge = tmpData.Y.at(0);
	startFrame.heave = tmpData.Z.at(0);
	startFrame.yaw = tmpRotData.X.at(0);
	startFrame.pitch = tmpRotData.Y.at(0);
	startFrame.roll = tmpRotData.Z.at(0);
	MovePlatform(&startFrame);

	m_recordOffset = m_data.X.size();

	// Copy all the stuff found the temp vectors to the end of the main data vectors.
	for (int i = 0; i < static_cast<int>(tmpData.X.size()); i++) {
		m_data.X.push_back(tmpData.X.at(i));
		m_data.Y.push_back(tmpData.Y.at(i));
#if DOF_MODE
		m_data.Z.push_back(tmpData.Z.at(i) + MOTION_BASE_CENTER);
#else
		m_data.Z.push_back(tmpData.Z.at(i));
#endif
		m_rotData.X.push_back(tmpRotData.X.at(i));
		m_rotData.Y.push_back(tmpRotData.Y.at(i));
		m_rotData.Z.push_back(tmpRotData.Z.at(i));
	}

//#if !CUSTOM_TIMER && !RECORD_MODE - Johnny
#if !RECORD_MODE
	if(!m_customTimer){
		m_delay = g_pList.GetVectorData("SYNC_DELAY").at(0);
		SyncNextFrame();
	}
#endif
}

void MoogDotsCom::CalFakeRotation(Point3 rotCenter, Vector3 rotVector, vector<double> rotAngles, Point3 FP)
{
	// eyeOffsets and headCenter are Moog coord system.
	vector<double> eyeOffsets = g_pList.GetVectorData("EYE_OFFSETS");
	vector<double> headCenter = g_pList.GetVectorData("HEAD_CENTER");
	//double eyeSeparation = g_pList.GetVectorData("IO_DIST")[0];						// Distance between eyes.
	double eyeSeparation = 0;
	double camera2screenDist = CENTER2SCREEN - eyeOffsets.at(1) - headCenter.at(1);	// Distance from monkey to screen.
	Point3	eyeLoc, rotFP,	// world coord system 
			FPeye, rotFPeye;	// camera coord system
	Vector3 eyeLoc2FP, eyeLoc2rotFP, eyeRotVector, FP2rotFP, up(0.0, 1.0, 0.0); 

	//Origin of OpenGL on XY plane is at two eyes center.
	//if(whichEye==LEFT_EYE) eyeLoc.set(-eyeSeparation/2.0, 0.0, camera2screenDist);
	//else eyeLoc.set(eyeSeparation/2.0, 0.0, camera2screenDist); // right eye
	eyeLoc.set(0.0, 0.0, camera2screenDist);

	Point3 prevRotFP;
	vector<double> rotAngleVelocities, fakeObjVelocities;
	double maxFPposVelocity = 0, maxRotAngleVelocity = 0, FPvelocity = 0, angleVelocity = 0;
	Affine4Matrix rotMatrix, tranMatrix1, tranMatrix2;
	Vector3 maxFPvelocity;
	for(int i=0; i<(int)rotAngles.size(); i++)
	{
		//Parital code from render() function
		//glTranslated(m_centerX, m_centerY, m_centerZ);
		//glRotated(m_rotationAngle, m_rotationVector.x, m_rotationVector.y, m_rotationVector.z);
		//glTranslated(-m_centerX, -m_centerY, -m_centerZ);
		//Find FP location after rotation that must same order matrix in Render() function (above)
		tranMatrix1.setTranslationMatrix(rotCenter.x, rotCenter.y, rotCenter.z);
		tranMatrix2.setTranslationMatrix(-rotCenter.x, -rotCenter.y, -rotCenter.z);
		rotMatrix.setRotationMatrix(rotVector,rotAngles.at(i)*DEG2RAD); // rotVector will normalize inside the function
		rotFP = tranMatrix1*rotMatrix*tranMatrix2*FP;
		//Notice: rotVector is for Moog rotation, so rotAngles is negative direction.
		//m_glRotData.push_back(-angleTrajectory.at(i));
		if(i>0) 
		{	//rotAngleVelocities is in absolute value. We compute the direction later.
			rotAngleVelocities.push_back(fabs(rotAngles.at(i)-rotAngles.at(i-1)));
			FPvelocity = sqrt((rotFP.x-prevRotFP.x)*(rotFP.x-prevRotFP.x)+(rotFP.y-prevRotFP.y)*(rotFP.y-prevRotFP.y));
			if(maxRotAngleVelocity < rotAngleVelocities.back()) maxRotAngleVelocity = rotAngleVelocities.back();
		}

		if(maxFPposVelocity < FPvelocity) 
		{
			maxFPposVelocity = FPvelocity;
			maxFPvelocity.x = rotFP.x-prevRotFP.x; //include direction information
			maxFPvelocity.y = rotFP.y-prevRotFP.y;
		}
		prevRotFP = rotFP;
	}

	double ratio = maxFPposVelocity/maxRotAngleVelocity/100; //FP location is in cm. We change it to meter for object movement.
	for(int i=0; i<(int)rotAngleVelocities.size(); i++)
		fakeObjVelocities.push_back(rotAngleVelocities.at(i)*ratio);

	maxFPvelocity.normalize();

	int objNum = 0;
	nmClearMovementData(&m_glDynObjData[objNum]);
	m_glDynObjData[objNum].X.push_back(0);
	m_glDynObjData[objNum].Y.push_back(0);
	m_glDynObjData[objNum].Z.push_back(0);
	for(int i=0; i<(int)fakeObjVelocities.size();i++)
	{	//Input rotAngles has already gone through transfer function, so we can directly use for m_glDynObjData.
		//object should move in opposite direction of FP moving direction.
		m_glDynObjData[objNum].X.push_back(m_glDynObjData[objNum].X.back()+fakeObjVelocities.at(i)*-maxFPvelocity.x);
		m_glDynObjData[objNum].Y.push_back(m_glDynObjData[objNum].Y.back()+fakeObjVelocities.at(i)*-maxFPvelocity.y);
		m_glDynObjData[objNum].Z.push_back(0);
	}

#if USE_MATLAB
	stuffDoubleVector(m_glDynObjData[objNum].X, "fakeObjLateral");	
#endif

	m_glWindow->GetGLPanel()->SetObjFieldTran(objNum,
											m_glDynObjData[objNum].X.at(0)*100.0,
											m_glDynObjData[objNum].Y.at(0)*100.0,
											m_glDynObjData[objNum].Z.at(0)*100.0);

}


void MoogDotsCom::CalEyeRotation(int whichEye, Point3 rotCenter, Vector3 rotVector, vector<double> rotAngles, Point3 FP)
{
	// eyeOffsets and headCenter are Moog coord system.
	vector<double> eyeOffsets = g_pList.GetVectorData("EYE_OFFSETS");
	vector<double> headCenter = g_pList.GetVectorData("HEAD_CENTER");
	double eyeSeparation = g_pList.GetVectorData("IO_DIST")[0];						// Distance between eyes.
	double camera2screenDist = CENTER2SCREEN - eyeOffsets.at(1) - headCenter.at(1);	// Distance from monkey to screen.
	Point3	eyeLoc, rotFP,	// world coord system 
			FPeye, rotFPeye;	// camera coord system
	Vector3 eyeLoc2FP, eyeLoc2rotFP, eyeRotVector, FP2rotFP, up(0.0, 1.0, 0.0); 

	//Origin of OpenGL on XY plane is at two eyes center.
	if(whichEye==LEFT_EYE) eyeLoc.set(-eyeSeparation/2.0, 0.0, camera2screenDist);
	else eyeLoc.set(eyeSeparation/2.0, 0.0, camera2screenDist); // right eye

	//Find matrix such that convert world coord to camera coordinate
	Affine4Matrix world2CameraMatrix, rotCameraCoordMatrix;
	world2CameraMatrix.setWorld2CameraMatrix(eyeLoc, FP, up);
	//Following prove that world2CameraMatrix == modelMatrix 
	//glMatrixMode(GL_MODELVIEW);
	//glLoadIdentity();
	//gluLookAt(eyeLoc.x, eyeLoc.y, eyeLoc.z,		// Camera origin
	//			FP.x, FP.y, FP.z,	// Camera direction
	//			0.0, 1.0, 0.0); // Which way is up
	//GLdouble modelMatrix[16];
	//glGetDoublev(GL_MODELVIEW_MATRIX, modelMatrix);

	m_rotCameraCoordMatrixVector[whichEye].clear();
	//FP.set(0.0, 0.0, 0.0);
	//up.set(0.0, 1.0, 0.0);

	Affine4Matrix rotMatrix, tranMatrix1, tranMatrix2, restoreCameraMatrix;
	for(int i=0; i<(int)rotAngles.size(); i++)
	{
		//Parital code from render() function
		//glTranslated(m_centerX, m_centerY, m_centerZ);
		//glRotated(m_rotationAngle, m_rotationVector.x, m_rotationVector.y, m_rotationVector.z);
		//glTranslated(-m_centerX, -m_centerY, -m_centerZ);
		//Find FP location after rotation that must same order matrix in Render() function (above)
		tranMatrix1.setTranslationMatrix(rotCenter.x, rotCenter.y, rotCenter.z);
		tranMatrix2.setTranslationMatrix(-rotCenter.x, -rotCenter.y, -rotCenter.z);
		rotMatrix.setRotationMatrix(rotVector,rotAngles.at(i)*DEG2RAD); // rotVector will normalize inside the function
		rotFP = tranMatrix1*rotMatrix*tranMatrix2*FP;
		//Notice: rotVector is for Moog rotation, so rotAngles is negative direction.
		//m_glRotData.push_back(-angleTrajectory.at(i));

		//Find location of FP and rotFP in eye coord
		FPeye = world2CameraMatrix*FP;
		rotFPeye = world2CameraMatrix*rotFP;
		//Find matrix such that it rotate the camera coord system towards rotFP.
		rotCameraCoordMatrix.setWorld2CameraMatrix(Point3(0.0,0.0,0.0), rotFPeye, up);
		//Prove that look = (0 0 z) at rotated camera coord system
		//Point3 look = rotCameraCoordMatrix*world2CameraMatrix*rotFP;

		m_rotCameraCoordMatrixVector[whichEye].push_back(rotCameraCoordMatrix);
	}
}

void MoogDotsCom::CalEyeMask(int whichEye, Point3 rotCenter, Vector3 rotVector, vector<double> rotAngles, Point3 FP)
{
	GLdouble eyeSeparation, tiny = 0.0000153; //cm; the depth buffer has less than 16-bit resolution;
	Frustum *f = m_glWindow->GetGLPanel()->GetFrustum();
	int rotFPtype = (int)g_pList.GetVectorData("FP_ROTATE").at(0);
	Point3	eyeLoc,	// world coord system
			rotFPstart, rotFPend, rotFPdiff, rotFP;	
	Vector3 up(0.0, 1.0, 0.0); 

	m_eyeMaskVector[whichEye].clear();

	if(whichEye==LEFT_EYE) eyeSeparation = -f->eyeSeparation/2.0;
	else eyeSeparation = f->eyeSeparation/2.0;

	//Find FP location after rotation that must same order matrix in Render() function.
	Affine4Matrix rotMatrix, tranMatrix1, tranMatrix2;
	tranMatrix1.setTranslationMatrix(rotCenter.x, rotCenter.y, rotCenter.z);
	tranMatrix2.setTranslationMatrix(-rotCenter.x, -rotCenter.y, -rotCenter.z);
	rotMatrix.setRotationMatrix(rotVector,rotAngles.front()*DEG2RAD); // rotVector will normalize inside the function
	rotFPstart = tranMatrix1*rotMatrix*tranMatrix2*FP;
	rotMatrix.setRotationMatrix(rotVector,rotAngles.back()*DEG2RAD); // rotVector will normalize inside the function
	rotFPend = tranMatrix1*rotMatrix*tranMatrix2*FP;
	rotFPdiff = rotFPend - rotFPstart;

	//Find near clip plane (world coord system)
	GLdouble nearMask, top, bottom, right, left;
	//Following calculation is based on eye position. 
	nearMask = f->clipNear + tiny;
	top = (nearMask / f->camera2screenDist) * (f->screenHeight / 2.0f - f->worldOffsetZ);
	bottom = (nearMask / f->camera2screenDist) * (-f->screenHeight / 2.0f - f->worldOffsetZ);
	right = (nearMask / f->camera2screenDist) * (f->screenWidth / 2.0f - eyeSeparation - f->worldOffsetX);
	left = (nearMask / f->camera2screenDist) * (-f->screenWidth / 2.0f - eyeSeparation - f->worldOffsetX);
	//We don't change top and bottom, because eye at center of up and down.
	//We need find right, left and maskZ in world coord system
	right += eyeSeparation;
	left += eyeSeparation;
	GLdouble maskZ = f->camera2screenDist - nearMask; //fixed depth z for mask

	enum{ RIGHT, LEFT, TOP, BOTTOM};
	Point3 maskQuad[4][4]; // follow right hand rule and start point is right upper corner.
	GLdouble maskOffset[4]; // inner mask right, left, top and bottom distance to whole screen
	for(int i=0; i<(int)rotAngles.size(); i++)
	{
		if(rotFPtype == 2)
		{
			rotMatrix.setRotationMatrix(rotVector,rotAngles.at(i)*DEG2RAD); // rotVector will normalize inside the function
			rotFP = tranMatrix1*rotMatrix*tranMatrix2*FP;
		}
		//else if(rotFPtype == 3 || rotFPtype == 4) rotFP = FP; // FP always at center in simulation case.
		else rotFP = FP; // FP always at center in simulation case.

		// Find mask offset; We only care positive value here.
		// Fact: maskOffset[RIGHT] + maskOffset[LEFT] = rotFPdiff, 
		// That means the size of inner window is constant.
		if(rotFPdiff.x > 0.0) // FP rotate from left to right
		{
			maskOffset[RIGHT] = rotFPdiff.x - (rotFP.x - rotFPstart.x);
			maskOffset[LEFT] = rotFP.x - rotFPstart.x;
		}
		else // FP rotate from right to left
		{
			maskOffset[LEFT] = -(rotFPdiff.x - (rotFP.x - rotFPstart.x));
			maskOffset[RIGHT] = -(rotFP.x - rotFPstart.x);
		}

		if(rotFPdiff.y > 0.0) // FP rotate from bottom to top
		{
			maskOffset[TOP] = rotFPdiff.y - (rotFP.y - rotFPstart.y);
			maskOffset[BOTTOM] = rotFP.y - rotFPstart.y;
		}
		else // FP rotate from right to left
		{
			maskOffset[BOTTOM]= -(rotFPdiff.y - (rotFP.y - rotFPstart.y));
			maskOffset[TOP] = -(rotFP.y - rotFPstart.y); 
		}

		//---Right side quad---//
		//right upper corner
		maskQuad[RIGHT][0].x = right; maskQuad[RIGHT][0].y = top; maskQuad[RIGHT][0].z = maskZ;
		//left upper corner
		maskQuad[RIGHT][1].x = (nearMask / f->camera2screenDist) * ((f->screenWidth / 2.0f - eyeSeparation - f->worldOffsetX)- maskOffset[RIGHT]) + eyeSeparation;
		maskQuad[RIGHT][1].y = top; maskQuad[RIGHT][1].z = maskZ;
		//left lower corner
		maskQuad[RIGHT][2].x = maskQuad[RIGHT][1].x; maskQuad[RIGHT][2].y = bottom; maskQuad[RIGHT][2].z = maskZ;
		//right lower corner
		maskQuad[RIGHT][3].x = right; maskQuad[RIGHT][3].y = bottom; maskQuad[RIGHT][3].z = maskZ;

		//---Left side quad---//
		//right upper corner
		maskQuad[LEFT][0].x = (nearMask / f->camera2screenDist) * ((-f->screenWidth / 2.0f - eyeSeparation - f->worldOffsetX) + maskOffset[LEFT]) + eyeSeparation; 
		maskQuad[LEFT][0].y = top; maskQuad[LEFT][0].z = maskZ;
		//left upper corner
		maskQuad[LEFT][1].x = left;
		maskQuad[LEFT][1].y = top; maskQuad[LEFT][1].z = maskZ;
		//left lower corner
		maskQuad[LEFT][2].x = left; maskQuad[LEFT][2].y = bottom; maskQuad[LEFT][2].z = maskZ;
		//right lower corner
		maskQuad[LEFT][3].x = maskQuad[LEFT][0].x; maskQuad[LEFT][3].y = bottom; maskQuad[LEFT][3].z = maskZ;

		//---Top side quad---//
		//right upper corner
		maskQuad[TOP][0].x = right; maskQuad[TOP][0].y = top; maskQuad[TOP][0].z = maskZ;
		//left upper corner
		maskQuad[TOP][1].x = left; maskQuad[TOP][1].y = top; maskQuad[TOP][1].z = maskZ;
		//left lower corner
		maskQuad[TOP][2].x = left; 
		maskQuad[TOP][2].y = (nearMask / f->camera2screenDist) * ((f->screenHeight / 2.0f - f->worldOffsetZ) - maskOffset[TOP]); 
		maskQuad[TOP][2].z = maskZ;
		//right lower corner
		maskQuad[TOP][3].x = right; maskQuad[TOP][3].y = maskQuad[TOP][2].y; maskQuad[TOP][3].z = maskZ;

		//---Bottom side quad---//
		//right upper corner
		maskQuad[BOTTOM][0].x = right; 
		maskQuad[BOTTOM][0].y = (nearMask / f->camera2screenDist) * ((-f->screenHeight / 2.0f - f->worldOffsetZ) + maskOffset[BOTTOM]); 
		maskQuad[BOTTOM][0].z = maskZ;
		//left upper corner
		maskQuad[BOTTOM][1].x = left; maskQuad[BOTTOM][1].y = maskQuad[BOTTOM][0].y; maskQuad[BOTTOM][1].z = maskZ;
		//left lower corner
		maskQuad[BOTTOM][2].x = left; maskQuad[BOTTOM][2].y = bottom; maskQuad[BOTTOM][2].z = maskZ;
		//right lower corner
		maskQuad[BOTTOM][3].x = right; maskQuad[BOTTOM][3].y = bottom; maskQuad[BOTTOM][3].z = maskZ;

		EyeMask eyeMask;
		GLint count=0;
		Point3 worldPoint;
		for(int j=0; j<4; j++)
		{
			for(int k=0; k<4; k++)
			{
				//worldPoint = camera2WorldMatrix*maskQuad[j][k];
				eyeMask.quadsMaskVertex3D[count++] = maskQuad[j][k].x;
				eyeMask.quadsMaskVertex3D[count++] = maskQuad[j][k].y;
				eyeMask.quadsMaskVertex3D[count++] = maskQuad[j][k].z;
			}
		}
		
		eyeMask.count = count;
		m_eyeMaskVector[whichEye].push_back(eyeMask);

		//if(rotFPtype == 3 || rotFPtype == 4) break; // In simulation case, FP is fixed in center and mask also is fixed.
		if(rotFPtype != 2) break; // In simulation case, FP is fixed in center and mask also is fixed.
	}

}

//jw 5/26/20006
void MoogDotsCom::CalculateStimulusOutput()
{
	double magnitude = g_pList.GetVectorData("M_DIST").at(1);
	double duration = g_pList.GetVectorData("M_TIME").at(1);
	double sigma = g_pList.GetVectorData("M_SIGMA").at(1);

	// Calculate position trajectory for CED.
	nmGen1DVGaussTrajectory(&m_gaussianTrajectoryData, magnitude, duration, 60.0, sigma, 0.0, true);	
	m_interpStimulusData = DifferenceFunc(LATERAL_POLE, LATERAL_ZERO, m_gaussianTrajectoryData,	"predCED", true);
	nmGenDerivativeCurve(&m_StimulusData, &m_interpStimulusData, 1/60.0, true);

	// Normalize the output data.
	vector<double> normFactors = g_pList.GetVectorData("STIM_NORM_FACTORS");
	nmNormalizeVector(&m_StimulusData, normFactors.at(0), normFactors.at(1));
}


// jw add 05/26/2006
void MoogDotsCom::OutputStimulusCurve(int index)
{
	int ulStat;
	WORD DataValue;

	if(index >= static_cast<int>(m_StimulusData.size())) {
		ulStat = cbFromEngUnits(m_PCI_DIO48H_Object.DIO_board_num, BIP10VOLTS, 0.0, &DataValue);
		ulStat = cbAOut(m_PCI_DIO48H_Object.DIO_board_num,0, BIP10VOLTS, DataValue);	
	}
	else {
		float EngUnits;
		EngUnits = m_StimulusData[index];
		ulStat = cbFromEngUnits(m_PCI_DIO48H_Object.DIO_board_num, BIP10VOLTS, EngUnits, &DataValue);
		ulStat = cbAOut(m_PCI_DIO48H_Object.DIO_board_num,0, BIP10VOLTS, DataValue);	
	}
}


// jw add 05/26/2006
void MoogDotsCom::AddOverlap2Inerval(int mFirstEndIndex,int glFirstEndIndex,int overlapCnt)
{
	// calculate 2 platform overlap interval movement with 2 zero delay inerval
	int endIndex, Index2,Index1;
    
	Index1 = mFirstEndIndex-overlapCnt;
    Index2=mFirstEndIndex;
	
	for(Index1;Index1<=mFirstEndIndex;Index1++)
	{
		Index2++;
		m_data.X.at(Index1)=m_data.X.at(Index1)+m_data.X.at(Index2)-m_data.X.at(mFirstEndIndex) ;
		m_data.Y.at(Index1)=m_data.Y.at(Index1)+m_data.Y.at(Index2)-m_data.Y.at(mFirstEndIndex);
		m_data.Z.at(Index1)=m_data.Z.at(Index1)+m_data.Z.at(Index2)-m_data.Z.at(mFirstEndIndex);
	}
	//move non overlap 2nd interval part to position
    Index2++;
	endIndex = m_data.X.size()-1;
	Index1 =mFirstEndIndex;
	for(Index2;Index2<=endIndex;Index2++)
	{
        Index1++;	
		m_data.X.at(Index1)=m_data.X.at(Index2);
		m_data.Y.at(Index1)=m_data.Y.at(Index2);
		m_data.Z.at(Index1)=m_data.Z.at(Index2);
	}

	for(Index1;Index1<endIndex;Index1++)
	{
		m_data.X.pop_back();
		m_data.Y.pop_back();
		m_data.Z.pop_back();
	}
   // Add-glWindow Data
	Index1 = glFirstEndIndex-overlapCnt;
    Index2=glFirstEndIndex;
	
	for(Index1;Index1<=glFirstEndIndex;Index1++)
	{
		Index2++;
		m_glData.X.at(Index1)=m_glData.X.at(Index1)+m_glData.X.at(Index2)-m_glData.X.at(glFirstEndIndex) ;
		m_glData.Y.at(Index1)=m_glData.Y.at(Index1)+m_glData.Y.at(Index2)-m_glData.Y.at(glFirstEndIndex);
		m_glData.Z.at(Index1)=m_glData.Z.at(Index1)+m_glData.Z.at(Index2)-m_glData.Z.at(glFirstEndIndex);
	}
	//move non overlap 2nd interval part to position
    Index2++;
	endIndex = m_glData.X.size()-1;
	Index1 =glFirstEndIndex;
	for(Index2;Index2<=endIndex;Index2++)
	{
        Index1++;	
		m_glData.X.at(Index1)=m_glData.X.at(Index2);
		m_glData.Y.at(Index1)=m_glData.Y.at(Index2);
		m_glData.Z.at(Index1)=m_glData.Z.at(Index2);
	}
	for(Index1;Index1<endIndex;Index1++)
	{
		m_glData.X.pop_back();
		m_glData.Y.pop_back();
		m_glData.Z.pop_back();
	}
}


void MoogDotsCom::MovePlatform(DATA_FRAME *destination)
{
	// Empty the data vectors, which stores the trajectory data.
	nmClearMovementData(&m_data);
	nmClearMovementData(&m_rotData);

	// Get the positions currently in the command buffer.  We use the thread safe
	// version of GetAxesPositions() here because MovePlatform() is called from
	// both the main GUI thread and the communication thread.
	DATA_FRAME currentFrame;
	GET_DATA_FRAME(&currentFrame);

#if DOF_MODE
	// We assume that the heave value passed to us is based around zero.  We must add an offset
	// to that value to adjust for the Moog's inherent offset on the heave axis.
	destination->heave += MOTION_BASE_CENTER;
#endif

	// Check to see if the motion base's current position is the same as the startPosition.  If so,
	// we don't need to move the base into position.
	if (fabs(destination->lateral - currentFrame.lateral) > TINY_NUMBER ||
		fabs(destination->surge - currentFrame.surge) > TINY_NUMBER ||
		fabs(destination->heave - currentFrame.heave) > TINY_NUMBER)
	{
		// Move the platform from its current location to start position.
		nm3DDatum sp, ep;
		sp.x = currentFrame.lateral; sp.y = currentFrame.surge; sp.z = currentFrame.heave;
		ep.x = destination->lateral; ep.y = destination->surge; ep.z = destination->heave;
		nmGen3DVGaussTrajectory(&m_data, sp, ep, 2.0, 60.0, 3.0, false);
	}

	// Make sure that we're not rotated at all.
	if (fabs(destination->yaw - currentFrame.yaw) > TINY_NUMBER ||
		fabs(destination->pitch - currentFrame.pitch) > TINY_NUMBER ||
		fabs(destination->roll - currentFrame.roll) > TINY_NUMBER)
	{
		// Set the Yaw.
		nmGen1DVGaussTrajectory(&m_rotData.X, destination->yaw-currentFrame.yaw, 2.0, 60.0, 3.0, currentFrame.yaw, false);

		// Set the Pitch.
		nmGen1DVGaussTrajectory(&m_rotData.Y, destination->pitch-currentFrame.pitch, 2.0, 60.0, 3.0, currentFrame.pitch, false);

		// Set the Roll.
		nmGen1DVGaussTrajectory(&m_rotData.Z, destination->roll-currentFrame.roll, 2.0, 60.0, 3.0, currentFrame.roll, false);
	}

	// Now we make sure that data in m_data and data in m_rotData has the same length.
	if (m_data.X.size() > m_rotData.X.size()) {
		for (int i = 0; i < (int)m_data.X.size(); i++) {
			m_rotData.X.push_back(currentFrame.yaw);
			m_rotData.Y.push_back(currentFrame.pitch);
			m_rotData.Z.push_back(currentFrame.roll);
		}
	}
	else if (m_data.X.size() < m_rotData.X.size()) {
		for (int i = 0; i < (int)m_rotData.X.size(); i++) {
			m_data.X.push_back(currentFrame.lateral);
			m_data.Y.push_back(currentFrame.surge);
			m_data.Z.push_back(currentFrame.heave);
		}
	}
}


void MoogDotsCom::Calculate2IntervalMovement()
{
	int i;
	double div = 60.0;

	m_continuousMode = false;

	// This tells Compute() not to set any rotation info and the GLPanel not to try
	// to do any rotation transformations in Render().
	m_setRotation = false;
	m_glWindow->GetGLPanel()->DoRotation(false);

	// Make sure we don't rotate the fixation point.
	m_glWindow->GetGLPanel()->RotationType(0);

	// Do no move these initializations.  Their location in the function is very important for
	// threading issues.
	m_grabIndex = 0;
	m_recordIndex = 0;

	// Move the platform into starting position.
	DATA_FRAME startFrame;
	vector<double> o = g_pList.GetVectorData("M_ORIGIN");
	startFrame.lateral = o.at(0);
	startFrame.surge = o.at(1);
	startFrame.heave = o.at(2);
	startFrame.yaw = 0.0f;
	startFrame.pitch = 0.0f;
	startFrame.roll = 0.0f;
	MovePlatform(&startFrame);

	m_recordOffset = m_data.X.size();

	// Grab the parameters we need for the calculations.
	vector<double> elevation = g_pList.GetVectorData("2I_ELEVATION"),
				   azimuth = g_pList.GetVectorData("2I_AZIMUTH"),
				   sigma = g_pList.GetVectorData("2I_SIGMA"),
				   duration = g_pList.GetVectorData("2I_TIME"),
				   magnitude = g_pList.GetVectorData("2I_DIST");

	// Calculate the 1st movement for the platform.
	nm3DDatum offsets;
	offsets.x = startFrame.lateral; offsets.y = startFrame.surge; offsets.z = startFrame.heave;
	nmGen3DVGaussTrajectory(&m_data, elevation.at(0), azimuth.at(0), magnitude.at(0),
						   duration.at(0), div, sigma.at(0), offsets, true, false);

	// 1st movement for OpenGL.
	nmGen3DVGaussTrajectory(&m_glData, elevation.at(2), azimuth.at(2), magnitude.at(2),
						   duration.at(2), div, sigma.at(2), offsets, true, true);

	// Add in the delay for the platform.
	vector<double> delays = g_pList.GetVectorData("2I_DELAY");
	int delayCount = static_cast<int>(delays.at(0)*div);
	int endIndex = static_cast<int>(m_data.X.size())-1;
	double lastValueX = m_data.X.at(endIndex),
		   lastValueY = m_data.Y.at(endIndex),
		   lastValueZ = m_data.Z.at(endIndex);
	for (i = 0; i < delayCount; i++) {
		m_data.X.push_back(lastValueX);
		m_data.Y.push_back(lastValueY);
		m_data.Z.push_back(lastValueZ);
	}

	// Now add in the delay for OpenGL.
	delayCount = static_cast<int>(delays.at(1)*div);
	endIndex = static_cast<int>(m_glData.X.size())-1;
	lastValueX = m_glData.X.at(endIndex);
	lastValueY = m_glData.Y.at(endIndex);
	lastValueZ = m_glData.Z.at(endIndex);
	for (i = 0; i < delayCount; i++) {
		m_glData.X.push_back(lastValueX);
		m_glData.Y.push_back(lastValueY);
		m_glData.Z.push_back(lastValueZ);
	}

	// 2nd movement for the platform.
	endIndex = static_cast<int>(m_data.X.size())-1;
	offsets.x = m_data.X.at(endIndex); offsets.y = m_data.Y.at(endIndex); offsets.z = m_data.Z.at(endIndex);
	nmGen3DVGaussTrajectory(&m_data, elevation.at(1), azimuth.at(1), magnitude.at(1),
						   duration.at(1), div, sigma.at(1), offsets, true, false);

	// 2nd movement for OpenGL.
	endIndex = static_cast<int>(m_glData.X.size())-1;
	offsets.x = m_glData.X.at(endIndex); offsets.y = m_glData.Y.at(endIndex); offsets.z = m_glData.Z.at(endIndex);
	nmGen3DVGaussTrajectory(&m_glData, elevation.at(3), azimuth.at(3), magnitude.at(3),
						   duration.at(3), div, sigma.at(3), offsets, true, false);

	// Make sure the yaw, pitch, and roll components are filled with zeros since we don't
	// want those to move.
	for (i = m_recordOffset; i < static_cast<int>(m_data.X.size()); i++) {
		m_rotData.X.push_back(0.0);
		m_rotData.Y.push_back(0.0);
		m_rotData.Z.push_back(0.0);
	}
	// Creates interpolated, predicted data based on the command signal in m_glData.
	GeneratePredictedData();

//#if !CUSTOM_TIMER && !RECORD_MODE - Johnny
#if !RECORD_MODE
	if(!m_customTimer){
		m_delay = g_pList.GetVectorData("SYNC_DELAY").at(0);
		SyncNextFrame();
	}
#endif
}

vector<double> MoogDotsCom::FindTrapezoidVelocityTraj(double s, double t, double t1, double t2)
{
	vector<double> trapezoidVelocityTraj;
	//double s, s1, s2, t, t1, t2, ti, step, a1, a2, v;
	double s1, s2, ti, step, a1, a2, v;

	// use Gaussian parameters
	//s = g_pList.GetVectorData("M_DIST").at(mode);
	//t = g_pList.GetVectorData("M_TIME").at(mode); // sec
	//t1 = g_pList.GetVectorData("TRAPEZOID_TIME1").at(mode)*t; // sec
	//t2 = g_pList.GetVectorData("TRAPEZOID_TIME2").at(mode)*t; // sec
	step = 1.0/60.0;

	//Check error input
	if(t1 > t2 || t2 >= t)
	{
		int i=0;
		while(i*step <= t) 
		{	
			trapezoidVelocityTraj.push_back(0.0);
			i++;
		}
		if(t1 > t2)
			m_messageConsole->Append(wxString::Format("Error: start time (%f) >= end time (%f) in trapesoid const. velocity", t1, t2));
		else
			m_messageConsole->Append(wxString::Format("Error: end time (%f) >= duration time (%f) in trapesoid const. velocity", t2, t));
		
		return trapezoidVelocityTraj;
	}

	if(t1 < 0.01) t1 = 0.01;
	if(t2 > t) t2 = t-0.01;

	//Equations of uniformly accelerated linear motion
	//s = the distance between initial and final positions (displacement)
    //u = the initial velocity (speed in a given direction)
    //v = the final velocity
    //a = the constant acceleration
    //t = the time taken to move from the initial state to the final state 
	//v = u + at
	//s = 1/2(u + v)t
	//s = ut + (1/2)at^2
	
	//For constant velocity
	//s = vt

	//Let t be duration,
	//Let t1 be the start time of constant trapezoid velocity.
	//Let t2 be the end time of constant trapezoid velocity.
	//Let v be the constant trapezoid velocity.
	//Let s be the total distance (magnitude).
	//Let s1 be the distance between duration 0->t1.
	//Let s2 be the distance between duration t1->t2.
	//Let s3 be the distance between duration t2->t.
	//Where s = s1+s2+s3
	//Give: s, t, t1, t2. Find v?
	//s1 = 1/2*(v*t1); s2 = v*(t2-t1); s3 = 1/2*v*(t-t3)
	//s = s1+s2+s3 = 1/2*v*(t+t2-t1)
	//v = 2*s/(t+t2-t1);
	v = 2*s/(t+t2-t1); //constant trapezoid velocity

	//Find trajectory between duration 0->t1.
	int i = 0;
	a1 = v/t1; // constant acceleration between duration 0->t1.
	while(i*step <= t1) 
	{	// s = ut + (1/2)at^2, where u=0
		trapezoidVelocityTraj.push_back(0.5*a1*i*step*i*step);
		i++;
	}
	
	//Find trajectory between duration t1->t2.
	s1 = 0.5*(v*t1); //s = 1/2(u + v)t
	//For triangle velocity (t2==t1), this step will skip.
	while(i*step <= t2) 
	{	
		ti = i*step - t1;
		trapezoidVelocityTraj.push_back(v*ti+s1); //si = v*ti + s1
		i++;
	}

	//Find trajectory between duration t2->t.
	s2 = v*(t2-t1); //s=vt for constant v.
	a2 = -v/(t-t2); // constant deceleration between duration t2->t.
	while(i*step <= t) 
	{	
		ti = i*step - t2; 
		// s = ut + (1/2)at^2, where u=0
		trapezoidVelocityTraj.push_back(v*ti + 0.5*a2*ti*ti + s1 + s2);
		i++;
	}

#if USE_MATLAB
	stuffDoubleVector(trapezoidVelocityTraj, "TVT");
#endif

	if (m_verboseMode) 
		m_messageConsole->Append(wxString::Format("Trapezoid acce.(m/s^2): a1=%f, a2=%f", a1, a2));

	return trapezoidVelocityTraj;
}

void MoogDotsCom::CalculateTrapezoidVelocityMovement()
{
	m_continuousMode = false;

	// This tells Compute() not to set any rotation info and the GLPanel not to try
	// to do any rotation transformations in Render().
	m_setRotation = false;
	m_glWindow->GetGLPanel()->DoRotation(false);

	// Make sure we don't rotate the fixation point.
	m_glWindow->GetGLPanel()->RotationType(0);

	// Do no move these initializations.  Their location in the function is very important for
	// threading issues.
	m_grabIndex = 0;
	m_recordIndex = 0;

	// Clear the OpenGL data.
	nmClearMovementData(&m_glData);

	/******************* calculate trapezoid velocity trajectory of platform ********************/
	nmMovementData tmpTraj;
	vector<double> trapezoidVelocityTraj;
	vector<double> startPoint;
	double elevation, azimuth;

	trapezoidVelocityTraj = FindTrapezoidVelocityTraj(	
								g_pList.GetVectorData("M_DIST").at(0), 
								g_pList.GetVectorData("M_TIME").at(0), // sec
								g_pList.GetVectorData("TRAPEZOID_TIME1").at(0)*g_pList.GetVectorData("M_TIME").at(0), // sec
								g_pList.GetVectorData("TRAPEZOID_TIME2").at(0)*g_pList.GetVectorData("M_TIME").at(0)); // sec
	startPoint = g_pList.GetVectorData("M_ORIGIN");
	// use trapezoidVelocityTraj to setup Moog data
	elevation = g_pList.GetVectorData("M_ELEVATION").at(0);
	azimuth = g_pList.GetVectorData("M_AZIMUTH").at(0);
	int s = trapezoidVelocityTraj.size();
	for (int j = 0; j < s; j++) {
		//find x, y and z component
		nm3DDatum cv = nmSpherical2Cartesian(elevation, azimuth, trapezoidVelocityTraj.at(j), true);
		tmpTraj.X.push_back(cv.x + startPoint.at(0));
		tmpTraj.Y.push_back(cv.y + startPoint.at(1));
#if DOF_MODE
		tmpTraj.Z.push_back(cv.z + startPoint.at(2) + MOTION_BASE_CENTER);
#else
		tmpTraj.Z.push_back(cv.z + startPoint.at(2));
#endif
	}

	// Calculates the trajectory to move the platform to start position.
	DATA_FRAME startFrame;
	startFrame.lateral = tmpTraj.X.at(0); startFrame.surge = tmpTraj.Y.at(0); startFrame.heave = tmpTraj.Z.at(0)-MOTION_BASE_CENTER;
	startFrame.yaw = startFrame.pitch = startFrame.roll = 0.0;
	MovePlatform(&startFrame);

	m_recordOffset = static_cast<int>(m_data.X.size());

	// Add the triangle velocity trajectory.
	for (int i = 0; i < static_cast<int>(tmpTraj.X.size()); i++) {
		m_data.X.push_back(tmpTraj.X.at(i));
		m_data.Y.push_back(tmpTraj.Y.at(i));
		m_data.Z.push_back(tmpTraj.Z.at(i));
		m_rotData.X.push_back(0.0);
		m_rotData.Y.push_back(0.0);
		m_rotData.Z.push_back(0.0);
	}

	/*************************** Calculate the trapezoid Velocity for OpenGL. ************************/
	trapezoidVelocityTraj = FindTrapezoidVelocityTraj(	
								g_pList.GetVectorData("M_DIST").at(1), 
								g_pList.GetVectorData("M_TIME").at(1), // sec
								g_pList.GetVectorData("TRAPEZOID_TIME1").at(1)*g_pList.GetVectorData("M_TIME").at(1), // sec
								g_pList.GetVectorData("TRAPEZOID_TIME2").at(1)*g_pList.GetVectorData("M_TIME").at(1)); // sec
	elevation = g_pList.GetVectorData("M_ELEVATION").at(1);
	azimuth = g_pList.GetVectorData("M_AZIMUTH").at(1);
	s = trapezoidVelocityTraj.size();
	for (int j = 0; j < s; j++) {
		//find x, y and z component
		nm3DDatum cv = nmSpherical2Cartesian(elevation, azimuth, trapezoidVelocityTraj.at(j), true);
		m_glData.X.push_back(cv.x + startPoint.at(0));
		m_glData.Y.push_back(cv.y + startPoint.at(1));
#if DOF_MODE
		m_glData.Z.push_back(cv.z + startPoint.at(2) + MOTION_BASE_CENTER);
#else
		m_glData.Z.push_back(cv.z + startPoint.at(2));
#endif
	}

	GeneratePredictedData();
}

void MoogDotsCom::CalculateGaussianMovement(DATA_FRAME *startFrame, double elevation,
											double azimuth, double magnitude, double duration,
											double sigma, bool doSecondMovement)
{
	int i;

	m_continuousMode = false;

	// This tells Compute() not to set any rotation info and the GLPanel not to try
	// to do any rotation transformations in Render().
	m_setRotation = false;
	m_glWindow->GetGLPanel()->DoRotation(false);

	// Make sure we don't rotate the fixation point.
	m_glWindow->GetGLPanel()->RotationType(0);

	// Do no move these initializations.  Their location in the function is very important for
	// threading issues.
	m_grabIndex = 0;
	m_recordIndex = 0;

	// Clear the noise data.
	nmClearMovementData(&m_noise);
	nmClearMovementData(&m_filteredNoise);

#if USE_MATLAB
	// Values that are only really used when taking debug and feedback data through Matlab.
#if RECORD_MODE
	m_recordedLateral.clear(); m_recordedHeave.clear(); m_recordedSurge.clear();
	m_recordedYaw.clear(); m_recordedPitch.clear(); m_recordedRoll.clear();
#endif
	m_sendStamp.clear(); m_receiveStamp.clear();
#endif

	// Moves the platform to start position.
	MovePlatform(startFrame);

	m_recordOffset = m_data.X.size();

	// Sometimes we just want to move the motion base to a space fixed location.  In this
	// case, we don't make a second movement.
	if (doSecondMovement) {
		// Generate the trajectory for the platform.
		nm3DDatum offset;
		offset.x = startFrame->lateral; offset.y = startFrame->surge; offset.z = startFrame->heave;
		nmGen3DVGaussTrajectory(&m_data, elevation, azimuth, magnitude, duration, 60.0, sigma, offset, true, false);

		// GL scene movement.
		vector<double> startPoint = g_pList.GetVectorData("M_ORIGIN");
		offset.x = startPoint.at(0); offset.y = startPoint.at(1); offset.z = startPoint.at(2);
#if DOF_MODE
		// We assume that the heave value passed to us is based around zero.  We must add an offset
		// to that value to adjust for the Moog's inherent offset on the heave axis.
		offset.z += MOTION_BASE_CENTER;
#endif
		elevation = g_pList.GetVectorData("M_ELEVATION").at(1);
		azimuth = g_pList.GetVectorData("M_AZIMUTH").at(1);
		magnitude = g_pList.GetVectorData("M_DIST").at(1);
		duration = g_pList.GetVectorData("M_TIME").at(1);
		sigma = g_pList.GetVectorData("M_SIGMA").at(1);
		nmGen3DVGaussTrajectory(&m_glData, elevation, azimuth, magnitude, duration, 60.0, sigma, offset, true, true);

		// Add noise to the signal if flagged.
		if (g_pList.GetVectorData("USE_NOISE").at(0)) {
			vector<double> noiseMag = g_pList.GetVectorData("NOISE_MAGNITUDE");
			nm3DDatum mag;
			mag.x = noiseMag.at(0); mag.y = noiseMag.at(1); mag.z = noiseMag.at(2);

			// Generate the filtered noise that we'll add to the command buffer.
			nmGenerateFilteredNoise((long)g_pList.GetVectorData("GAUSSIAN_SEED").at(0),
									(int)m_data.X.size() - m_recordOffset,
									g_pList.GetVectorData("CUTOFF_FREQ").at(0),
									mag, 1, true, true,
									&m_noise, &m_filteredNoise);

			// This is a function from NumericalMethods that will rotate a data set.
			nmRotateDataYZ(&m_filteredNoise, g_pList.GetVectorData("NOISE_AZIMUTH").at(0),
						   g_pList.GetVectorData("NOISE_ELEVATION").at(0));

			// Add the noise to the command and visual feed.
			for (i = 0; i < static_cast<int>(m_filteredNoise.X.size()); i++) {
				int index = i + m_recordOffset;

				// Command
				m_data.X.at(index) += m_filteredNoise.X.at(i);
				m_data.Y.at(index) += m_filteredNoise.Y.at(i);
				m_data.Z.at(index) += m_filteredNoise.Z.at(i);

				// Visual
				m_glData.X.at(i) += m_filteredNoise.X.at(i);
				m_glData.Y.at(i) += m_filteredNoise.Y.at(i);
				m_glData.Z.at(i) += m_filteredNoise.Z.at(i);
			}
		}

		// Creates interpolated, predicted data based on the command signal in m_glData.
		GeneratePredictedData();

		// Make sure the yaw, pitch, and roll components are filled with zeros for the
		// 2nd part of the movement.
		for (i = m_recordOffset; i < static_cast<int>(m_data.X.size()); i++) {
			m_rotData.X.push_back(0.0);
			m_rotData.Y.push_back(0.0);
			m_rotData.Z.push_back(0.0);
		}
	} // End if (doSecondMovement)

//#if !CUSTOM_TIMER - Johnny 6/17/07
	if(!m_customTimer){
		m_delay = g_pList.GetVectorData("SYNC_DELAY").at(0);
		SyncNextFrame();
	}
//#endif
}

void MoogDotsCom::CalculateGaussianObjectMovement(int objNum)
{
	int i = objNum;
	// GL Object movement uses Moog Coord to calculate, because it follows CalculateGaussianMovement() function.
	double elevation = g_pList.GetVectorData("OBJ_M_ELE").at(i);
	double azimuth = g_pList.GetVectorData("OBJ_M_AZI").at(i);
	double magnitude = g_pList.GetVectorData("OBJ_M_DIST").at(i*2);
	double duration = g_pList.GetVectorData("OBJ_M_TIME").at(i);
	double sigma = g_pList.GetVectorData("OBJ_M_SIGMA").at(i);
	double lumSigma = g_pList.GetVectorData("OBJ_DYN_LUM_SIGMA").at(i);

	// Moog coord system: x-Lateral, y-surge, z-heave
	nm3DDatum offset;
	XYZ3 objOrigin=m_glWindow->GetGLPanel()->glDynObject[objNum]->GetObjOrigin();
	offset.x = objOrigin.x; offset.y = objOrigin.z; offset.z = objOrigin.y;

	if(g_pList.GetVectorData("OBJ_M_DIST").at(i*2+1)==1.0) //in degree
	{//assume object move frontal parallel plane
		Frustum *f = m_glWindow->GetGLPanel()->GetFrustum();
		magnitude = (f->camera2screenDist-objOrigin.z*100.0)*tan(magnitude*PI/180.0)/100.0;
	}

	nmMovementData tmpData;
	nmGen3DVGaussTrajectory(&tmpData, elevation, azimuth, magnitude, duration, 60.0, sigma, offset, true, true);

	// GeneratePredictedData();
	m_glDynObjData[i].X = DifferenceFunc(LATERAL_POLE, LATERAL_ZERO, tmpData.X, "predObjLateral");
	m_glDynObjData[i].Y = DifferenceFunc(HEAVE_POLE, HEAVE_ZERO, tmpData.Z, "predObjHeave");
	m_glDynObjData[i].Z = DifferenceFunc(SURGE_POLE, SURGE_ZERO, tmpData.Y, "predObjSurge");

	m_glWindow->GetGLPanel()->SetObjFieldTran(objNum,
											m_glDynObjData[i].X.at(0)*100.0,
											m_glDynObjData[i].Y.at(0)*100.0,
											m_glDynObjData[i].Z.at(0)*100.0);

	// Setup luminance array
	vector<double> dominateDir;
	if(m_glWindow->GetGLPanel()->glDynObject[objNum]->GetObject().luminanceFlag==1.0)
	{
		nmGen3DVGaussTrajectory(&tmpData, elevation, azimuth, magnitude, duration, 60.0, lumSigma, offset, true, true);

		objDynLum[objNum].clear();
		//dominate direction
		nm3DDatum value = nmSpherical2Cartesian(elevation, azimuth, magnitude, true); // for moog coord
		value.x = fabs(value.x); value.y = fabs(value.z); value.z = fabs(value.y); // make all positive values in OpenGL coord
		if(value.x >= value.y)
		{	// tmpData in Moog coord
			if(value.x >= value.z) dominateDir = tmpData.X; //m_glDynObjData[objNum].X;
			else dominateDir = tmpData.Y; //m_glDynObjData[objNum].Z;
		}
		else // value.x < value.y
		{
			if(value.y >= value.z) dominateDir = tmpData.Z; //m_glDynObjData[objNum].Y;
			else dominateDir = tmpData.Y; //m_glDynObjData[objNum].Z;
		}
		
		double maxValue = 0.0, currValue;
		objDynLum[objNum].push_back(0.0);
		for(int di=1; di<(int)dominateDir.size(); di++)
		{
			currValue = fabs(dominateDir.at(di)-dominateDir.at(di-1));
			if(currValue > maxValue) maxValue = currValue;
			objDynLum[objNum].push_back(currValue);
		}
		for(int di=0; di<(int)objDynLum[objNum].size(); di++) objDynLum[objNum].at(di) = objDynLum[objNum].at(di)/maxValue; //normalize

		objDynLum[objNum].push_back(0.0);

		m_glWindow->GetGLPanel()->glDynObject[objNum]->SetDynLumRatio(0.0);

#if USE_MATLAB
	stuffDoubleVector(objDynLum[objNum], "objDynLum");
#endif
	}
	else
		m_glWindow->GetGLPanel()->glDynObject[objNum]->SetDynLumRatio(1.0);
}

void MoogDotsCom::CalculateTrapezoidVelocityObjectMovement(int objNum)
{
	int trajectory_length;
	// GL Object movement uses Moog Coord to calculate, because it follows CalculateSinMovement() function.
	// Moog coord system: x-Lateral, y-surge, z-heave
	nm3DDatum offset;
	XYZ3 objOrigin=m_glWindow->GetGLPanel()->glDynObject[objNum]->GetObjOrigin();
	offset.x = objOrigin.x; offset.y = objOrigin.z; offset.z = objOrigin.y;

	double magnitude = g_pList.GetVectorData("OBJ_M_DIST").at(objNum*2);
	double duration = g_pList.GetVectorData("OBJ_M_TIME").at(objNum);		//sec
	double trapezoidTime1 = g_pList.GetVectorData("TRAPEZOID_TIME1").at(1); //sec
	double trapezoidTime2 = g_pList.GetVectorData("TRAPEZOID_TIME2").at(1); //sec
	double elevation = g_pList.GetVectorData("OBJ_M_ELE").at(objNum);
	double azimuth = g_pList.GetVectorData("OBJ_M_AZI").at(objNum);
	vector<double> trapezoidVelocityTraj;
    trapezoidVelocityTraj = FindTrapezoidVelocityTraj(magnitude, duration, trapezoidTime1*duration, trapezoidTime2*duration);

#if USE_MATLAB
	stuffDoubleVector(trapezoidVelocityTraj, "trapezoidVelocityTraj");
#endif

	nmMovementData tmpData;
	trajectory_length = trapezoidVelocityTraj.size();
	for (int i = 0; i < trajectory_length; i++) {
		//find x, y and z component
		nm3DDatum cv = nmSpherical2Cartesian(elevation, azimuth, trapezoidVelocityTraj.at(i), true);
		tmpData.X.push_back(cv.x + offset.x);
		tmpData.Y.push_back(cv.y + offset.y);
		tmpData.Z.push_back(cv.z + offset.z);
	}

	// GeneratePredictedData();
	m_glDynObjData[objNum].X = DifferenceFunc(LATERAL_POLE, LATERAL_ZERO, tmpData.X, "predObjLateral");
	m_glDynObjData[objNum].Y = DifferenceFunc(HEAVE_POLE, HEAVE_ZERO, tmpData.Z, "predObjHeave");
	m_glDynObjData[objNum].Z = DifferenceFunc(SURGE_POLE, SURGE_ZERO, tmpData.Y, "predObjSurge");

	m_glWindow->GetGLPanel()->SetObjFieldTran(objNum,
											m_glDynObjData[objNum].X.at(0)*100.0,
											m_glDynObjData[objNum].Y.at(0)*100.0,
											m_glDynObjData[objNum].Z.at(0)*100.0);

}

void MoogDotsCom::CalculateSinObjectMovement(int objNum)
{
	int trajectory_length;
	// GL Object movement uses Moog Coord to calculate, because it follows CalculateSinMovement() function.
	// Moog coord system: x-Lateral, y-surge, z-heave
	nm3DDatum offset;
	XYZ3 objOrigin=m_glWindow->GetGLPanel()->glDynObject[objNum]->GetObjOrigin();
	offset.x = objOrigin.x; offset.y = objOrigin.z; offset.z = objOrigin.y;

	double magnitude = g_pList.GetVectorData("OBJ_M_DIST").at(objNum*2);
	double frequency = g_pList.GetVectorData("OBJ_M_FREQUENCY").at(objNum);
	double duration = g_pList.GetVectorData("OBJ_M_TIME").at(objNum);
	double pref_direction = g_pList.GetVectorData("OBJ_M_ELE").at(objNum);
	double sigma = g_pList.GetVectorData("OBJ_M_SIGMA").at(objNum);
	double exponent = g_pList.GetVectorData("OBJ_M_EXPONENT").at(objNum);
	double phase = g_pList.GetVectorData("OBJ_M_PHASE").at(objNum)*PI/180.0;
	vector<double> trajectory;

	if(g_pList.GetVectorData("OBJ_M_DIST").at(objNum*2+1)==1.0) //in degree
	{//assume object move frontal parallel plane
		Frustum *f = m_glWindow->GetGLPanel()->GetFrustum();
		magnitude = (f->camera2screenDist-objOrigin.z*100.0)*tan(magnitude*PI/180.0)/100.0;
	}
	double Xcomponent = cos(PI*pref_direction/180.0)*magnitude;
	double Zcomponent = -sin(PI*pref_direction/180.0)*magnitude;

	// Create gaussian window multiplied by sin wave
	trajectory_length = (int)(duration*60.0) + 1;
	double t0 = trajectory_length/2.0;
	double s = (t0/60.0)*sigma;
	for(int i=0;i<trajectory_length;i++){
		//trajectory.push_back(exp(-pow((i-t0),exponent)/(2*pow(s,exponent))) * sin(phase+(i/60.0)*2.0*PI*frequency));
		trajectory.push_back(exp(-pow((i-t0),exponent)/(2*pow(s,exponent))) * sin(phase+(i/(duration*60.0))*2.0*PI*frequency*duration));
	}

	// Create sin data structures
	nmMovementData tmpData;
	for(int i=0;i<trajectory_length;i++){
		tmpData.X.push_back(Xcomponent * trajectory[i] + offset.x);
		tmpData.Y.push_back(offset.y);
		tmpData.Z.push_back(Zcomponent * trajectory[i] + offset.z);
	}

	for (i = 0; i < 6; i++) {  // Pad on 6 frames of the last value to reduce truncation
		tmpData.X.push_back(tmpData.X.back());
		tmpData.Y.push_back(tmpData.Y.back());
		tmpData.Z.push_back(tmpData.Z.back());
	}
#if USE_MATLAB
	stuffDoubleVector(tmpData.X, "objLateral");
#endif
	// GeneratePredictedData();
	m_glDynObjData[objNum].X = DifferenceFunc(LATERAL_POLE, LATERAL_ZERO, tmpData.X, "predObjLateral");
	m_glDynObjData[objNum].Y = DifferenceFunc(HEAVE_POLE, HEAVE_ZERO, tmpData.Z, "predObjHeave");
	m_glDynObjData[objNum].Z = DifferenceFunc(SURGE_POLE, SURGE_ZERO, tmpData.Y, "predObjSurge");

	m_glWindow->GetGLPanel()->SetObjFieldTran(objNum,
											m_glDynObjData[objNum].X.at(0)*100.0,
											m_glDynObjData[objNum].Y.at(0)*100.0,
											m_glDynObjData[objNum].Z.at(0)*100.0);

}

Vector3 MoogDotsCom::CalculateObjMotionInDepthDir(int objNum)
{
	Point3 objInitPos, objEndPos, objInDepthPos, eyePos;
	Vector3 objInit2EndVector, objInit2eyeVector, planeNormal, objInit2InDepthVector;
	Affine4Matrix planeRotateMatrix;

	int obj_motion_type = g_pList.GetVectorData("OBJ_MOTION_TYPE").at(objNum);

	ObjStarField currObj=m_glWindow->GetGLPanel()->glDynObject[objNum]->GetObject();
	Frustum *f = m_glWindow->GetGLPanel()->GetFrustum();

	//All following calculation, we are based on Frustum(OpenGL) coord and in cm.
	objInitPos.set(currObj.origin.at(0)*100.0, currObj.origin.at(1)*100.0, currObj.origin.at(2)*100.0); //cm
	eyePos.set(0.0, 0.0, f->camera2screenDist);

	double elevation = g_pList.GetVectorData("OBJ_M_ELE").at(objNum);
	double azimuth = g_pList.GetVectorData("OBJ_M_AZI").at(objNum); //according to right hand rule of Moog coord (z-dir go upward); 
	double magnitude = g_pList.GetVectorData("OBJ_M_DIST").at(objNum); 
	double dirInDepthAngle = g_pList.GetVectorData("OBJ_DIR_IN_DEPTH").at(objNum);
	double lineSlope, magObjMoveInDepthForHalf, magObjMoveInDepthBackHalf, magObjMoveInDepthPrev, magObjMoveInDepth;

	if(g_pList.GetVectorData("OBJ_M_DIST").at(objNum*2+1)==1.0) //in degree
	{//assume object move frontal parallel plane
		magnitude = (f->camera2screenDist-currObj.origin.at(2)*100.0)*tan(magnitude*PI/180.0)/100.0; //meter
	}
	magnitude *= 100.0; //cm
	objInit2EndVector.set(	magnitude * cos(elevation*PI/180.0) * cos(-azimuth*PI/180.0), // x-componenet
							magnitude * sin(elevation*PI/180.0),					// y-componenet
							magnitude * cos(elevation*PI/180.0) * sin(-azimuth*PI/180.0));// z-componenet

	//If in depth angle is 0, then object moves in original direction and magnitude.
	//if(dirInDepthAngle == 0.0) return objInit2EndVector;

	objEndPos.set( objInitPos.x+objInit2EndVector.x, objInitPos.y+objInit2EndVector.y, objInitPos.z+objInit2EndVector.z);
	objInit2eyeVector.setDiff(eyePos,objInitPos); // objInit2eyeVector = eyePos - objInitPos;
	planeNormal = objInit2eyeVector.cross(objInit2EndVector);
	planeRotateMatrix.setRotationMatrix(planeNormal, dirInDepthAngle*PI/180.);
	//objInit2InDepthVector = planeRotateMatrix*objInit2EndVector;
	objInit2InDepthVector = planeRotateMatrix*objInit2eyeVector;
	objInit2InDepthVector.normalize();

	if (obj_motion_type == 3)	// constant retinal speed
	{
		//Check the objInit2InDepthVector and flip the objInit2EndVector if necessary.
		Vector3 normalLineVector = planeNormal.cross(objInit2eyeVector);
		normalLineVector.normalize();
		double tmp = normalLineVector.dot(objInit2InDepthVector);
		if(normalLineVector.dot(objInit2InDepthVector) < 0) //It means angle > 90 degree , so we need flip the objInit2EndVector.
		{
			objInit2EndVector = -objInit2EndVector;
		}
		//
		// First, compute the forward (on-going direction) half-length magnitude.
		//
		objEndPos.set( objInitPos.x+objInit2EndVector.x/2, objInitPos.y+objInit2EndVector.y/2, objInitPos.z+objInit2EndVector.z/2);
		//Using line equation(objEndPos & eyePos) and normal vertor(objInit2InDepthVector)
		//to find the final magnitude of object moving in depth.
		if (cos(elevation*PI/180.0) < 0.00001) {
			lineSlope = (eyePos.y-objEndPos.y)/(eyePos.z-objEndPos.z);
			magObjMoveInDepthForHalf = (-eyePos.y + objInitPos.y - lineSlope*objInitPos.z + lineSlope*eyePos.z)/(lineSlope*objInit2InDepthVector.z-objInit2InDepthVector.y);
		} else {
			lineSlope = (eyePos.x-objEndPos.x)/(eyePos.z-objEndPos.z);
			magObjMoveInDepthForHalf = (-eyePos.x + objInitPos.x - lineSlope*objInitPos.z + lineSlope*eyePos.z)/(lineSlope*objInit2InDepthVector.z-objInit2InDepthVector.x);
		}

		// Now, compute the backward (opposite direction) half-length magnitude
		objEndPos.set( objInitPos.x-objInit2EndVector.x/2, objInitPos.y-objInit2EndVector.y/2, objInitPos.z-objInit2EndVector.z/2);
		//Using line equation(objEndPos & eyePos) and normal vertor(objInit2InDepthVector)
		//to find the final magnitude of object moving in depth.
		if (cos(elevation*PI/180.0) < 0.00001) {
			lineSlope = (eyePos.y-objEndPos.y)/(eyePos.z-objEndPos.z);
			magObjMoveInDepthBackHalf = (-eyePos.y + objInitPos.y - lineSlope*objInitPos.z + lineSlope*eyePos.z)/(lineSlope*objInit2InDepthVector.z-objInit2InDepthVector.y);
		} else {
			lineSlope = (eyePos.x-objEndPos.x)/(eyePos.z-objEndPos.z);
			magObjMoveInDepthBackHalf = (-eyePos.x + objInitPos.x - lineSlope*objInitPos.z + lineSlope*eyePos.z)/(lineSlope*objInit2InDepthVector.z-objInit2InDepthVector.x);
		}

		// This is the previous method (just for comparing purpose)
		objEndPos.set( objInitPos.x+objInit2EndVector.x, objInitPos.y+objInit2EndVector.y, objInitPos.z+objInit2EndVector.z);
		//Using line equation(objEndPos & eyePos) and normal vertor(objInit2InDepthVector)
		//to find the final magnitude of object moving in depth.
		if (cos(elevation*PI/180.0) < 0.00001) {
			lineSlope = (eyePos.y-objEndPos.y)/(eyePos.z-objEndPos.z);
			magObjMoveInDepthPrev = (-eyePos.y + objInitPos.y - lineSlope*objInitPos.z + lineSlope*eyePos.z)/(lineSlope*objInit2InDepthVector.z-objInit2InDepthVector.y);
		} else {
			lineSlope = (eyePos.x-objEndPos.x)/(eyePos.z-objEndPos.z);
			magObjMoveInDepthPrev = (-eyePos.x + objInitPos.x - lineSlope*objInitPos.z + lineSlope*eyePos.z)/(lineSlope*objInit2InDepthVector.z-objInit2InDepthVector.x);
		}

		// check sign: magnitude for forward half should be positive, magnitude for backward half should be negative
		if(magObjMoveInDepthForHalf <= 0 || magObjMoveInDepthBackHalf >= 0)
		{
			m_messageConsole->InsertItems(1, &wxString::Format("Err: magObjMoveInDepthForHalf=%f, magObjMoveInDepthBackHalf=%f!", magObjMoveInDepthForHalf, magObjMoveInDepthBackHalf), 0);
			return Vector3();
		}
		magObjMoveInDepth = magObjMoveInDepthForHalf  -  magObjMoveInDepthBackHalf;

		// 
		// check by re-constructing target speed vector
		Point3 ObjMoveInDepthEndPos(objInit2InDepthVector.x*magObjMoveInDepthForHalf+objInitPos.x, 
					objInit2InDepthVector.y*magObjMoveInDepthForHalf+objInitPos.y, 
					objInit2InDepthVector.z*magObjMoveInDepthForHalf+objInitPos.z);

		objEndPos.set( objInitPos.x+objInit2EndVector.x/2, objInitPos.y+objInit2EndVector.y/2, objInitPos.z+objInit2EndVector.z/2);
		Vector3 tmpV1(eyePos.x-ObjMoveInDepthEndPos.x, eyePos.y-ObjMoveInDepthEndPos.y, eyePos.z-ObjMoveInDepthEndPos.z);
		Vector3 tmpV2(eyePos.x-objEndPos.x, eyePos.y-objEndPos.y, eyePos.z-objEndPos.z);
		Vector3 tmpV3 = tmpV1.cross(tmpV2);
		double result = tmpV3.magnitude();
		if(result >  0.001)
		{
			m_messageConsole->InsertItems(1, &wxString::Format("Err: vector computation error(%f). check internal routine", result), 0);
		}
	} // In case of constant world-fixed speed, use the original magnitude.
	else if (obj_motion_type == 4)
	{
		//magObjMoveInDepth = objInit2EndVector.magnitude();
		magObjMoveInDepth = magnitude; //cm
	}

	// for debug
	m_messageConsole->InsertItems(1, &wxString::Format("MagFor:%f, MagBack:%f Total:%f Prev:%f", magObjMoveInDepthForHalf, magObjMoveInDepthBackHalf, magObjMoveInDepth, magObjMoveInDepthPrev), 0);

	return objInit2InDepthVector*magObjMoveInDepth;
}


void MoogDotsCom::CalculateConstSpeedObjectMovement(int objNum)
{
	Vector3 objMotionInDepthDir = CalculateObjMotionInDepthDir(objNum);
	double movingDist = objMotionInDepthDir.magnitude();
	double duration = g_pList.GetVectorData("OBJ_M_TIME").at(objNum);
	double trajectory_length = (int)(duration*60.0) + 1;
	double magnitude = objMotionInDepthDir.magnitude();
	double speed = magnitude/(trajectory_length-1); //distance(cm) per frame
	objMotionInDepthDir.normalize();
	XYZ3 objOrigin=m_glWindow->GetGLPanel()->glDynObject[objNum]->GetObjOrigin();

	// wrong motion in depth angle, so make object disappear.
	if(magnitude == 0.0) objOrigin.x = 1000;

	//Only for OpenGl drawing, so we don't call DifferenceFunc().
	nmClearMovementData(&m_glDynObjData[objNum]); // in meter
	for(int i=0;i<trajectory_length;i++){
		//For debug,
		//double tmp = objOrigin.x*100.0 + i*speed*objMotionInDepthDir.x;
		m_glDynObjData[objNum].X.push_back(objOrigin.x + i*speed*objMotionInDepthDir.x/100.0); // change to meter
		m_glDynObjData[objNum].Y.push_back(objOrigin.y + i*speed*objMotionInDepthDir.y/100.0);
		m_glDynObjData[objNum].Z.push_back(objOrigin.z + i*speed*objMotionInDepthDir.z/100.0);
	}

	m_glWindow->GetGLPanel()->SetObjFieldTran(objNum,
											m_glDynObjData[objNum].X.at(0)*100.0,
											m_glDynObjData[objNum].Y.at(0)*100.0,
											m_glDynObjData[objNum].Z.at(0)*100.0);

#if USE_MATLAB
	stuffDoubleVector(m_glDynObjData[objNum].X, "m_glDynObjDataX");
	stuffDoubleVector(m_glDynObjData[objNum].Y, "m_glDynObjDataY");
	stuffDoubleVector(m_glDynObjData[objNum].Z, "m_glDynObjDataZ");
#endif

	//If the object motion very fast, it will run out the dots.
	//Rotating the object z-axis to align with object motion in depth direction
	//can partially solve this problem, when we increase the z-dimension volume.
	//Notice: whateven the direction of objRotVector vector, it will match the rotAngle. 
	Vector3 z_axis(0, 0, 1);
	Vector3 objRotVector = z_axis.cross(objMotionInDepthDir); //objMotionInDepthDir is unit vector.
	double rotAngle = acos(z_axis.dot(objMotionInDepthDir))*RAD2DEG;
	m_glWindow->GetGLPanel()->glDynObject[objNum]->SetObjRotAngle(rotAngle);
	objRotVector.normalize();
	m_glWindow->GetGLPanel()->glDynObject[objNum]->SetObjRotVector(objRotVector.x, objRotVector.y, objRotVector.z);
}


void MoogDotsCom::CalculatePseudoDepthObjectMovement(int objNum)
{
	//Assume that Moog trajectory was calculated.
	//Following is partial codes in Compute() function that set the camera location(
	//glPanel->SetLateral(m_interpLateral.at(m_grabIndex)*100.0);
	//glPanel->SetHeave(m_interpHeave.at(m_grabIndex)*100.0 + GL_OFFSET);
	//glPanel->SetSurge(m_interpSurge.at(m_grabIndex)*100.0);
	//Following is partial codes in Render() function that setup the camera.
	//gluLookAt(-m_frustum.eyeSeparation/2.0f+m_Lateral, 0.0f-m_Heave, m_frustum.camera2screenDist-m_Surge,		// Camera origin
	//		  -m_frustum.eyeSeparation/2.0f+m_Lateral, 0.0f-m_Heave, m_frustum.camera2screenDist-m_Surge-1.0f,	// Camera direction
	//		  0.0, 1.0, 0.0); // Which way is up

	//GL object movement use OpenGL coord to calculate.
	double z_ratio;
	Point3 cameraPos, objPos, pesudoObjPos; 
	XYZ3 objOrigin=m_glWindow->GetGLPanel()->glDynObject[objNum]->GetObjOrigin();
	objPos.set(objOrigin.x, objOrigin.y, objOrigin.z);
	//Find camera inital postion in meter.
	cameraPos.set(	m_interpLateral.at(0), 
					-(m_interpHeave.at(0)+GL_OFFSET/100.0), 
					m_glWindow->GetGLPanel()->GetFrustum()->camera2screenDist/100.0-m_interpSurge.at(0));
	//Find pseudo object inital position.
	pesudoObjPos.z = g_pList.GetVectorData("OBJ_PSEUDO_DEPTH").at(objNum); //meter
	if(g_pList.GetVectorData("OBJ_PSEUDO_DEPTH").at(objNum*2+1)==1.0) // use degree
		pesudoObjPos.z = m_glWindow->GetGLPanel()->DISPTODEPTH(pesudoObjPos.z)/100.0; //z-coord in meter that matches the OpenGL coord where screen center at (0,0,0).

	z_ratio = (pesudoObjPos.z-cameraPos.z)/(objPos.z-cameraPos.z); //Find the ratio in Z-coord. 
	pesudoObjPos.x = cameraPos.x+(objPos.x-cameraPos.x)*z_ratio;
	pesudoObjPos.y = cameraPos.y+(objPos.y-cameraPos.y)*z_ratio;

	nmClearMovementData(&m_glDynObjData[objNum]);
	//Find dynamic object location according to pseudo depath and camera position for each frame.
	//Here we use unit meter to do calulation.
	//double z_ratio;
	for(int i=0; i<(int)m_interpLateral.size(); i++)
	{
		//Set camera position in world coord-system
		cameraPos.set(	m_interpLateral.at(i), 
						-(m_interpHeave.at(i)+GL_OFFSET/100.0), 
						m_glWindow->GetGLPanel()->GetFrustum()->camera2screenDist/100.0-m_interpSurge.at(i));
		//We use triangle rule to find the object position according to pseudo depth.
		//We also assume that object only moves on frontal parallel plane, so Z-coord of object will not change.
		//We always subtract camera position to keep the sign correct and 
		//Find the ratio in Z-coord. 
		z_ratio = (objPos.z-cameraPos.z)/(pesudoObjPos.z-cameraPos.z);
		m_glDynObjData[objNum].X.push_back(cameraPos.x+(pesudoObjPos.x-cameraPos.x)*z_ratio); //
		m_glDynObjData[objNum].Y.push_back(cameraPos.y+(pesudoObjPos.y-cameraPos.y)*z_ratio); //Heave
		m_glDynObjData[objNum].Z.push_back(objPos.z);
	}

	// We should NOT call DifferenceFunc() for m_glDynObjData,
	// becsues camera position has already called DifferenceFunc() or GeneratePredictedData();

	//Initialize object position
	m_glWindow->GetGLPanel()->SetObjFieldTran(objNum,
											m_glDynObjData[objNum].X.at(0)*100.0,
											m_glDynObjData[objNum].Y.at(0)*100.0,
											m_glDynObjData[objNum].Z.at(0)*100.0);

#if USE_MATLAB
	stuffDoubleVector(m_glDynObjData[objNum].X, "pseudoObjLateral");
	stuffDoubleVector(m_glDynObjData[objNum].Y, "pseudoObjHeave");
	stuffDoubleVector(m_glDynObjData[objNum].Z, "pseudoObjSurge");
#endif
}

void MoogDotsCom::CalculateGaborMovement()
{
	m_continuousMode = false;

	// This tells Compute() not to set any rotation info and the GLPanel not to try
	// to do any rotation transformations in Render().
	m_setRotation = false;
	m_glWindow->GetGLPanel()->DoRotation(false);

	// Make sure we don't rotate the fixation point.
	m_glWindow->GetGLPanel()->RotationType(0);

	// Do no move these initializations.  Their location in the function is very important for
	// threading issues.
	m_grabIndex = 0;
	m_recordIndex = 0;

	// Clear the OpenGL data.
	nmClearMovementData(&m_glData);

	/******************* calculate gabor trajectory of platform ********************/
	nmMovementData tmpTraj;
	vector<double> gaborTraj;
	double elevation, azimuth, amp, duration, sigma, freq, cycle, step, t0, gauss,max;

	// use Gaussian parameters
	elevation = g_pList.GetVectorData("M_ELEVATION").at(0);
	azimuth = g_pList.GetVectorData("M_AZIMUTH").at(0);
	duration = g_pList.GetVectorData("M_TIME").at(0);
	sigma = g_pList.GetVectorData("M_SIGMA").at(0);
	// use Sinusoid parameters
	amp = g_pList.GetVectorData("SIN_TRANS_AMPLITUDE").at(0);
	freq = g_pList.GetVectorData("SIN_FREQUENCY").at(0);
	cycle = duration*freq;
	step = 1.0/60.0;
	t0 = duration/2.0;
	max = 0;

	for (double i = 0.0; i < duration + step; i += step) {
		double val = sin((i-t0)/duration*cycle* 2.0*PI);
		gauss = exp(-sqrt(2.0)*pow((i-t0)/(duration/sigma),2));
		gaborTraj.push_back(val*gauss);
		if (val*gauss > max) max = val*gauss;
	}

	int s = gaborTraj.size();
	for (int j = 0; j < s; j++) {
		// normalize to magnitude and find x, y and z component
		nm3DDatum cv = nmSpherical2Cartesian(elevation, azimuth, amp*gaborTraj.at(j)/max, true);
		tmpTraj.X.push_back(cv.x);
		tmpTraj.Y.push_back(cv.y);
#if DOF_MODE
		tmpTraj.Z.push_back(cv.z + MOTION_BASE_CENTER);
#else
		tmpTraj.Z.push_back(cv.z);
#endif
	}

#if USE_MATLAB
	stuffDoubleVector(tmpTraj.X, "tx");
	stuffDoubleVector(tmpTraj.Y, "ty");
	stuffDoubleVector(tmpTraj.Z, "tz");
#endif
	// Calculates the trajectory to move the platform to start position.
	DATA_FRAME startFrame;
	startFrame.lateral = tmpTraj.X.at(0); startFrame.surge = tmpTraj.Y.at(0); startFrame.heave = tmpTraj.Z.at(0)-MOTION_BASE_CENTER;
	startFrame.yaw = startFrame.pitch = startFrame.roll = 0.0;
	MovePlatform(&startFrame);

	m_recordOffset = static_cast<int>(m_data.X.size());

	// Add the gabor to the trajectory.
	for (int i = 0; i < static_cast<int>(tmpTraj.X.size()); i++) {
		m_data.X.push_back(tmpTraj.X.at(i));
		m_data.Y.push_back(tmpTraj.Y.at(i));
		m_data.Z.push_back(tmpTraj.Z.at(i));
		m_rotData.X.push_back(0.0);
		m_rotData.Y.push_back(0.0);
		m_rotData.Z.push_back(0.0);
	}

#if USE_MATLAB
	stuffDoubleVector(m_data.X, "dx");
	stuffDoubleVector(m_data.Y, "dy");
	stuffDoubleVector(m_data.Z, "dz");
#endif

	/*************************** Do the Gabor for OpenGL. ************************/
	// use Gaussian parameters
	elevation = g_pList.GetVectorData("M_ELEVATION").at(1);
	azimuth = g_pList.GetVectorData("M_AZIMUTH").at(1);
	duration = g_pList.GetVectorData("M_TIME").at(1);
	sigma = g_pList.GetVectorData("M_SIGMA").at(1);
	// use Sinusoid parameters
	amp = g_pList.GetVectorData("SIN_TRANS_AMPLITUDE").at(1);
	freq = g_pList.GetVectorData("SIN_FREQUENCY").at(1);
	cycle = duration*freq;
	t0 = duration/2.0;
	max=0;

	gaborTraj.clear();
	for (double i = 0.0; i < duration + step; i += step) {
		double val = sin((i-t0)/duration*cycle* 2.0*PI);
		gauss = exp(-sqrt(2.0)*pow((i-t0)/(duration/sigma),2));
		gaborTraj.push_back(val*gauss);
		if (val*gauss > max) max = val*gauss;
	}

	s = gaborTraj.size();
	for (int j = 0; j < s; j++) {
		// normalize to magnitude and find x, y and z component
		nm3DDatum cv = nmSpherical2Cartesian(elevation, azimuth, amp*gaborTraj.at(j)/max, true);
		m_glData.X.push_back(cv.x);
		m_glData.Y.push_back(cv.y);
#if DOF_MODE
		m_glData.Z.push_back(cv.z + MOTION_BASE_CENTER);
#else
		m_glData.Z.push_back(cv.z);
#endif
	}

	GeneratePredictedData();

#if USE_MATLAB
	stuffDoubleVector(m_glData.X, "gx");
	stuffDoubleVector(m_glData.Y, "gy");
	stuffDoubleVector(m_glData.Z, "gz");
#endif

}

void MoogDotsCom::GeneratePredictedData()
{
	// Clear the data structures which hold the interpolated return feedback.
	m_interpHeave.clear(); m_interpSurge.clear(); m_interpLateral.clear();
	m_accelerLateral.clear(); m_accelerHeave.clear(); m_accelerSurge.clear();

	m_interpLateral = DifferenceFunc(LATERAL_POLE, LATERAL_ZERO, m_glData.X, "predLateral");
	m_accelerLateral = m_accelerData;
	m_interpHeave = DifferenceFunc(HEAVE_POLE, HEAVE_ZERO, m_glData.Z, "predHeave");
	m_accelerHeave = m_accelerData;
	m_interpSurge = DifferenceFunc(SURGE_POLE, SURGE_ZERO, m_glData.Y, "predSurge");
	m_accelerSurge = m_accelerData;
	
	//Set all visual stimulus frame on by default
	m_stimOnData.clear();
	for(int i=0; i<(int)m_interpLateral.size(); i++) m_stimOnData.push_back(true);

}

void MoogDotsCom::CalVelocitySignal(Vector3 velocityDir)
{
	// Create velocity analog output 
	double multiplerValue=0.0, maxValue=0.0, maxAngleDiff=0.0, magnitude, angle;
	Vector3 currVelocity;
	
	// Get sign and magnitude from multipler
	double multipler = g_pList.GetVectorData("STIM_ANALOG_MULT").at(0);
	multiplerValue = fabs(multipler);

	m_velocityData.clear();

	vector<double> angleDiff;
	for(int i=0; i<(int)m_accelerLateral.size()-1; i++)
	{
		currVelocity.set(m_accelerLateral.at(i+1)-m_accelerLateral.at(i), m_accelerHeave.at(i+1)-m_accelerHeave.at(i), m_accelerSurge.at(i+1)-m_accelerSurge.at(i));
		magnitude = currVelocity.magnitude();
		currVelocity.normalize();
		if(magnitude==0.0) m_velocityData.push_back(0.0);
		else if(multipler*currVelocity.dot(velocityDir)>0) // positive sign
		{
			m_velocityData.push_back(magnitude);
			if(maxValue < magnitude) maxValue = magnitude;

			//For checking the angle differen? always 90 degree, when velocity = 0;
			angle = acos(currVelocity.dot(velocityDir))*180/PI;
			angleDiff.push_back(angle);
			//if(maxAngleDiff < angle) maxAngleDiff = angle;

		}
		else m_velocityData.push_back(0.0); //all negative value will be rectified.
	}

	//m_messageConsole->InsertItems(1, &wxString::Format("Max Angle Diff = %f", angle), 0);

	if(maxValue!=0.0)
	{
		for(int i=0; i<(int)m_velocityData.size(); i++)
		{	//Normalize and multipler
			m_velocityData.at(i) = multiplerValue*m_velocityData.at(i)/maxValue;
			//prevent overflow
			if(m_velocityData.at(i) > 4.0) m_velocityData.at(i) = 4.0;
		}
	}

#if USE_MATLAB
	stuffDoubleVector(m_velocityData, "m_velocityData");
	stuffDoubleVector(angleDiff, "angleDiff");
#endif
}


void MoogDotsCom::GeneratePredictedRotationData()
{
	int len;
	double *ypred, *tmppred, *i_ypred;

	double poleTerm = 0.1043*2.0*60.0,
		   zeroTerm = 0.0373*2.0*60.0;

	// Clear the rotation data.
	m_interpRotation.clear();

	len = static_cast<int>(m_glRotData.size());
	ypred = new double[len];

	// Since, as of this time, we have no transfer function for rotation,
	// we'll simply copy m_glRotData into ypred.
	for (int i = 0; i < len; i++) {
		ypred[i] = m_glRotData.at(i);
	}

	// Initialize the first value.
	ypred[0] = m_glRotData.at(0);

	// Here's Johnny...
	for (i = 1; i < len; i++) {
		ypred[i] = (1/(1+poleTerm)) * (-ypred[i-1]*(1-poleTerm) + m_glRotData.at(i)*(1+zeroTerm) +
					m_glRotData.at(i-1)*(1-zeroTerm));
	}

	// Interpolate the data.
	int interp_len, pred_len;
	tmppred = linear_interp(ypred, len, 10, interp_len);
	pred_len = static_cast<int>(500.0/16.667*10.0);
	i_ypred = new double[interp_len + pred_len];

	// Pad the interpolated data.
	for (i = 0; i < pred_len; i++) {
		i_ypred[i] = tmppred[0];
	}
	for (i = pred_len; i < pred_len + interp_len; i++) {
		i_ypred[i] = tmppred[i-pred_len];
	}

	// Stuff the interpolated data into a vector.
	int offset = static_cast<int>(g_pList.GetVectorData("PRED_OFFSET").at(0) / 16.667 * 10.0);
	for (i = offset; i < interp_len + pred_len; i += 10) {
		m_interpRotation.push_back(i_ypred[i]);
	}

	// Simulate accelerometer signal and then output by cbAOut() - Johnny 5/2/07
	m_accelerRotation.clear();
	int frame_delay = static_cast<int>(g_pList.GetVectorData("FRAME_DELAY")[0] / 16.667 * 10.0);
	offset = offset - frame_delay;
	for (i = offset; i < interp_len + pred_len; i += 10) {
		m_accelerRotation.push_back(i_ypred[i]);
	}

	// Delete dynamically allocated objects.
	delete [] tmppred;
	delete [] ypred;
	delete [] i_ypred;
}


vector<double> MoogDotsCom::DifferenceFunc(double pole, double zero, vector<double> x, string title, bool stimulusOffset)
{
	int i, len;
	double poleTerm, zeroTerm;
	double *ypred, *tmppred, *i_ypred;
	vector<double> interpData;
	//vector<double> x;
	//string title;
	bool isHeave = false;

	poleTerm = pole*2.0*60.0;
	zeroTerm = zero*2.0*60.0;
/*
	switch (axis)
	{
	case Axis::Heave:
		x = m_glData.Z;
		isHeave = true;
		title = "predHeave";
		break;
	case Axis::Lateral:
		x = m_glData.X;
		title = "predLateral";
		break;
	case Axis::Surge:
		x = m_glData.Y;
		title = "predSurge";
		break;
	case Axis::Stimulus:
		x = m_gaussianTrajectoryData;
		title = "predCED";
		break;
	};
*/
	len = static_cast<int>(x.size());
	ypred = new double[len];

	// Initialize the first value.
	ypred[0] = x[0];

	// Here's Johnny...
	for (i = 1; i < len; i++) {
		ypred[i] = (1/(1+poleTerm)) * (-ypred[i-1]*(1-poleTerm) + x[i]*(1+zeroTerm) +
					x[i-1]*(1-zeroTerm));
	}

#if USE_MATLAB
	vector<double> test;
	for (i = 0; i < len; i++) {
		test.push_back(ypred[i]);
	}
	stuffDoubleVector(test, title.c_str());
#endif

	// Interpolate the data.
	int interp_len, pred_len;
	tmppred = linear_interp(ypred, len, 10, interp_len);
	pred_len = static_cast<int>(500.0/16.667*10.0);
	i_ypred = new double[interp_len + pred_len];

	// Pad the interpolated data.
	for (i = 0; i < pred_len; i++) {
		i_ypred[i] = tmppred[0];
	}
	for (i = pred_len; i < pred_len + interp_len; i++) {
		i_ypred[i] = tmppred[i-pred_len];
	}

	// Stuff the interpolated data into a vector.
	int offset = static_cast<int>(g_pList.GetVectorData("PRED_OFFSET")[0] / 16.667 * 10.0);
	if(stimulusOffset){
      offset = static_cast<int>(g_pList.GetVectorData("PRED_OFFSET_STIMULUS")[0] / 16.667 * 10.0);
	}

	for (i = offset; i < interp_len + pred_len; i += 10) {
		interpData.push_back(i_ypred[i]);
	}

	// Simulate accelerometer signal and then output by cbAOut() - Johnny 5/2/07
	m_accelerData.clear();
	int frame_delay = static_cast<int>(g_pList.GetVectorData("FRAME_DELAY")[0] / 16.667 * 10.0);
	offset = offset - frame_delay;
	for (i = offset; i < interp_len + pred_len; i += 10) {
		m_accelerData.push_back(i_ypred[i]);
	}

/* for test only
#if USE_MATLAB
	vector<double> test2;
	for (i = 0; i < pred_len + interp_len; i++) {
		test2.push_back(i_ypred[i]);
	}
	stuffDoubleVector(test2, "i_ypred");

	test2.clear();
	for (i = 0; i < (int)interpData.size(); i++) {
		test2.push_back(interpData.at(i));
	}
	stuffDoubleVector(test2, "interpData");

	test2.clear();
	for (i = 0; i < (int)m_accelerData.size(); i++) {
		test2.push_back(m_accelerData.at(i));
	}
	stuffDoubleVector(test2, "m_accelerData");
#endif
*/
	// Delete dynamically allocated objects.
	delete [] tmppred;
	delete [] ypred;
	delete [] i_ypred;

	return interpData;
}


vector<double> MoogDotsCom::convertPolar2Vector(double elevation, double azimuth, double magnitude)
{
	vector<double> convertedVector;
	double x, y, z;

	// Calculate the z-component.
	z = magnitude * sin(elevation);

	// Calculate the y-component.
	y = magnitude * cos(elevation) * sin(azimuth);

	// Calculate the x-componenet.
	x = magnitude * cos(elevation) * cos(azimuth);

	// Stuff the results into a vector.
	convertedVector.push_back(x);
	convertedVector.push_back(y);
	convertedVector.push_back(z);

	return convertedVector;
}


double MoogDotsCom::deg2rad(double deg)
{
	return deg / 180 * PI;
}

void MoogDotsCom::ConnectPipes()
{
	// If we're using pipes, wait for a connection.
	if (m_connectionType == MoogDotsCom::ConnectionType::Pipes) {
		ConnectNamedPipe(m_pipeHandle, &m_overlappedEvent);

		// Wait for the pipe to get signaled.
		//m_messageConsole->InsertItems(1, &wxString("Waiting for client connection..."), 0);
		wxBusyInfo wait("Waiting for client connection...");
		WaitForSingleObject(m_overlappedEvent.hEvent, INFINITE);
		m_messageConsole->InsertItems(1, &wxString("Connected Pipes!"), 0);

		// Check the result.
		DWORD junk;
		if (GetOverlappedResult(m_pipeHandle, &m_overlappedEvent, &junk, FALSE) == 0) {
			wxMessageDialog d(NULL, "GetOverlappedResult failed.");
			d.ShowModal();
		}

		ResetEvent(m_overlappedEvent.hEvent);
	}
}

void MoogDotsCom::SetFirstPortBcbDOut()
{
	// (1 1 1) is in order of (B2 B1 B0)
	if (m_grabIndex+2 >= (int)m_interpLateral.size() ||
		m_data.index >= static_cast<int>(m_data.X.size()) ){	// Before Stop
		cbDOut(PULSE_OUT_BOARDNUM, FIRSTPORTB, 7);		//(111) - B0, B1 & B2 are ON.
		::Sleep(1);
		cbDOut(PULSE_OUT_BOARDNUM, FIRSTPORTB, 0);		//(000) - reset port B
		//::MessageBox(NULL,"Before Stop","Setup Port B", 0);
	}
	else if (m_data.index == m_recordOffset + 1) {		// Start frame
		cbDOut(PULSE_OUT_BOARDNUM, FIRSTPORTB, 7);		//(111) - B0, B1 & B2 are ON.
		::Sleep(1);
		cbDOut(PULSE_OUT_BOARDNUM, FIRSTPORTB, 4);		//(100) - Only B2 is ON.
		//::MessageBox(NULL,"Start frame","Setup Port B", 0);
	}
	// Send out a sync pulse only after the 1st frame of the trajectory has been
	// processed by the platform.  This equates to the 2nd time into this section
	// of the function. (for B0 only - Johnny 12/8/08)
	else if (m_data.index > m_recordOffset + 1) {		// After start
		//cbDOut(PULSE_OUT_BOARDNUM, FIRSTPORTB, 1);
		//cbDOut(PULSE_OUT_BOARDNUM, FIRSTPORTB, 0);
		cbDOut(PULSE_OUT_BOARDNUM, FIRSTPORTB, 5);		//(101) - B0 & B2 are ON and B1 is OFF
		::Sleep(1);
		cbDOut(PULSE_OUT_BOARDNUM, FIRSTPORTB, 4);		//(100) - Only B2 is ON
	}
}

void MoogDotsCom::CalculateSinMovement()
{
	// Do everything twice, first for MOOG, second for cameras.
	vector<double> startPosition = g_pList.GetVectorData("M_ORIGIN");
	double magnitude = g_pList.GetVectorData("MOVE_MAGNITUDE").at(0), magnitude_cam = g_pList.GetVectorData("MOVE_MAGNITUDE").at(1);
	double frequency = g_pList.GetVectorData("MOVE_FREQUENCY").at(0), frequency_cam = g_pList.GetVectorData("MOVE_FREQUENCY").at(1);
	double duration = g_pList.GetVectorData("MOVE_DURATION").at(0);
	double pref_direction = g_pList.GetVectorData("PREF_DIRECTION").at(0), pref_direction_cam = g_pList.GetVectorData("PREF_DIRECTION").at(1);
	double sigma = g_pList.GetVectorData("MOVE_SIGMA").at(0), sigma_cam = g_pList.GetVectorData("MOVE_SIGMA").at(1);
	double exponent = g_pList.GetVectorData("MOVE_EXPONENT").at(0), exponent_cam = g_pList.GetVectorData("MOVE_EXPONENT").at(1);
	double phase = g_pList.GetVectorData("MOVE_PHASE").at(0)*PI/180.0, phase_cam = g_pList.GetVectorData("MOVE_PHASE").at(1)*PI/180.0;
	double Xcomponent = cos(PI*pref_direction/180.0)*magnitude, Xcomponent_cam = cos(PI*pref_direction_cam/180.0)*magnitude_cam,
		   Zcomponent = -sin(PI*pref_direction/180.0)*magnitude, Zcomponent_cam = -sin(PI*pref_direction_cam/180.0)*magnitude_cam;
	int i, trajectory_length;
	double *trajectory, *trajectory_cam;

	// Add by Johnny - 12/9/08
	// This tells Compute() not to set any rotation info and the GLPanel not to try
	// to do any rotation transformations in Render().
	m_setRotation = false;
	m_glWindow->GetGLPanel()->DoRotation(false);

	// Make sure we don't rotate the fixation point.
	m_glWindow->GetGLPanel()->RotationType(0); //not sure? check it later!

	// Do not move these initializations.  Their location in the function is very important for
	// threading issues.
	m_grabIndex = 0;
	m_data.index = 0;
	m_recordIndex = 0;

	// Clear the OpenGL data.
	nmClearMovementData(&m_glData);

	m_continuousMode = false;

	// Empty the data vectors, which stores the trajectory data.
	//m_data.X.clear(); m_data.Y.clear(); m_data.Z.clear();
	//m_glData.X.clear(); m_glData.Y.clear(); m_glData.Z.clear();

#if USE_MATLAB
	// Values that are only really used when taking debug and feedback data through Matlab.
	m_recordedLateral.clear(); m_recordedHeave.clear(); m_recordedSurge.clear();
	m_sendStamp.clear(); m_receiveStamp.clear();
#endif

#if DOF_MODE
	startPosition[2] = MOTION_BASE_CENTER + startPosition[2];
#endif

	/*
	MoogFrame currentFrame;
	ThreadGetAxesPositions(&currentFrame);

	// Check to see if the motion base's current position is the same as the startPosition.  If so,
	// we don't need to move the base into position.
	if (fabs(startPosition[0] - currentFrame.lateral) > TINY_NUMBER ||
		fabs(startPosition[1] - currentFrame.surge) > TINY_NUMBER ||
		fabs(startPosition[2] - currentFrame.heave) > TINY_NUMBER) {
			// Grab the start position.
			vector<double> startPos;
			startPos.push_back(currentFrame.lateral); startPos.push_back(currentFrame.surge); startPos.push_back(currentFrame.heave);
	}

	// Offset the recording my the number of data points in the start movement so we don't record it.
	m_recordOffset = (int)m_data.X.size();
	*/

	// Create gaussian window multiplied by sin wave
	trajectory_length = (int)(duration*60.0) + 1;
	double t0 = trajectory_length/2.0;
	double s = (t0/60.0)*sigma, s_cam = (t0/60.0)*sigma_cam;
	trajectory = new double[trajectory_length];  trajectory_cam = new double[trajectory_length];
	if(duration==0){
		for(i=0;i<trajectory_length;i++)
			trajectory[i] = trajectory_cam[i] = 0.0;
	}
	else{
		for(i=0;i<trajectory_length;i++){
			//trajectory[i] = exp(-pow((i-t0),exponent)/(2*pow(s,exponent))) * sin(phase+(i/60.0)*2.0*PI*frequency);
			//trajectory_cam[i] = exp(-pow((i-t0),exponent_cam)/(2*pow(s_cam,exponent_cam))) * sin(phase_cam+(i/60.0)*2.0*PI*frequency_cam);
			trajectory[i] = exp(-pow((i-t0),exponent)/(2*pow(s,exponent))) * sin(phase+(i/(duration*60.0))*2.0*PI*frequency*duration);
			trajectory_cam[i] = exp(-pow((i-t0),exponent_cam)/(2*pow(s_cam,exponent_cam))) * sin(phase_cam+(i/(duration*60.0))*2.0*PI*frequency_cam*duration);
		}
	}

	// Calculates the trajectory to move the platform to start position. - Add by Johnny - 12/9/08
	DATA_FRAME startFrame;
	startFrame.lateral = Xcomponent * trajectory[0]; startFrame.surge = 0.0; startFrame.heave = Zcomponent * trajectory[0];
	startFrame.yaw = startFrame.pitch = startFrame.roll = 0.0;
	MovePlatform(&startFrame);

	m_recordOffset = static_cast<int>(m_data.X.size());


	// Create sin data structures m_data.X, Y and Z (MOOG) and m_interpHeave, Surge, and Lateral (cameras).
	for(i=0;i<trajectory_length;i++){
		m_data.X.push_back(Xcomponent * trajectory[i]);
		m_data.Y.push_back(0.0);
		m_data.Z.push_back(MOTION_BASE_CENTER + Zcomponent * trajectory[i]);
		m_rotData.X.push_back(0.0);
		m_rotData.Y.push_back(0.0);
		m_rotData.Z.push_back(0.0);
		//m_interpLateral.push_back(Xcomponent_cam * trajectory_cam[i]);
		//m_interpSurge.push_back(0.0);
		//m_interpHeave.push_back(MOTION_BASE_CENTER + Zcomponent_cam * trajectory_cam[i]);
		m_glData.X.push_back(Xcomponent_cam * trajectory_cam[i]);
		m_glData.Y.push_back(0.0);
		m_glData.Z.push_back(MOTION_BASE_CENTER + Zcomponent_cam * trajectory_cam[i]);
	}

	/*
	int lastIndex = trajectory_length-1;
	double lastX = m_data.X.at(lastIndex),
		   lastY = m_data.Y.at(lastIndex),
		   lastZ = m_data.Z.at(lastIndex),
		   glLastX = m_glData.X.at(lastIndex),
		   glLastY = m_glData.Y.at(lastIndex),
		   glLastZ = m_glData.Z.at(lastIndex);
	for (i = 0; i < 6; i++) {  // Pad on 6 frames of the last value to reduce truncation
		m_data.X.push_back(lastX); m_data.Y.push_back(lastY); m_data.Z.push_back(lastZ);
		m_glData.X.push_back(glLastX); m_glData.Y.push_back(glLastY); m_glData.Z.push_back(glLastZ);
	}
	*/
	for (i = 0; i < 6; i++) {  // Pad on 6 frames of the last value to reduce truncation
		m_data.X.push_back(m_data.X.back());
		m_data.Y.push_back(0.0);
		m_data.Z.push_back(m_data.Z.back());
		m_rotData.X.push_back(0.0);
		m_rotData.Y.push_back(0.0);
		m_rotData.Z.push_back(0.0);
		//m_interpLateral.push_back(Xcomponent_cam * trajectory_cam[i]);
		//m_interpSurge.push_back(0.0);
		//m_interpHeave.push_back(MOTION_BASE_CENTER + Zcomponent_cam * trajectory_cam[i]);
		m_glData.X.push_back(m_glData.X.back());
		m_glData.Y.push_back(0.0);
		m_glData.Z.push_back(m_glData.Z.back());
	}

	GeneratePredictedData();

	//CalVelocitySignal() must call after GeneratePredictedData()
	Vector3 velocityDir(Xcomponent, Zcomponent, 0.0);
	velocityDir.normalize();
	CalVelocitySignal(velocityDir);

	//Make sure it go back to original position after add 6 frame.

	//Motion parallax always moves on lateral direction, so we use it to find the turning point of motion.
	if((int)g_pList.GetVectorData("CTRL_STIM_ON").at(0) == 1)
	{
		int turnPointNum = 0;
		m_stimOnData.at(0) = false;
		i=1;
		//Find 1st and 2nd turning point
		while(i<(int)m_interpLateral.size()-2)
		{
			double a=m_interpLateral.at(i), b=m_interpLateral.at(i-1), c=m_interpLateral.at(i+1);
			if( (m_interpLateral.at(i) > m_interpLateral.at(i-1) && m_interpLateral.at(i) > m_interpLateral.at(i+1)) || // max point
				(m_interpLateral.at(i) < m_interpLateral.at(i-1) && m_interpLateral.at(i) < m_interpLateral.at(i+1)) ) // min point
			{
				m_stimOnOffIndex[turnPointNum] = i;
				turnPointNum++;
			}
			
			if(turnPointNum == 0) m_stimOnData.at(i) = false;
			//else if(turnPointNum == 1) m_stimOnData.at(i) = true; //By default, it is true.
			else if(turnPointNum == 2)
			{
				m_stimOnData.at(i) = true;
				i++;
				break;
			}

			i++;
		}

		while(i<(int)m_interpLateral.size()) m_stimOnData.at(i++) = false;
#if USE_MATLAB
		stuffDoubleVector(m_stimOnData, "m_stimOnData");
		//It can verify in Matlab command window
		//figure; plot(1:length(iLateral),iLateral,'.-', 1:length(iLateral),m_stimOnData.*iLateral,'or')
#endif
	}

	delete trajectory;
	delete trajectory_cam;
}

void MoogDotsCom::CreateWholeTraj(nmMovementData *trajectory, double duration, double onsetDelay)
{
	double step = 1/60.0;
	double t = 0.0;
	while(t < duration*onsetDelay)	// add onset delay period data in front
	{
		trajectory->X.insert(trajectory->X.begin(),trajectory->X.at(0));
		trajectory->Y.insert(trajectory->Y.begin(),trajectory->Y.at(0));
		trajectory->Z.insert(trajectory->Z.begin(),trajectory->Z.at(0));
		t += step;
	}
	t = (double)trajectory->X.size()*step;
	while(t <= duration) // add offset delay period data at back
	{
		trajectory->X.push_back(trajectory->X.back());
		trajectory->Y.push_back(trajectory->Y.back());
		trajectory->Z.push_back(trajectory->Z.back());
		t += step;
	}

	// When we input the onsetDelay=0.2, the program will read to 0.20000000000000001.
	// Then it is possible put one more data in front of trajectory, when onsetDelay>0.0 and offsetDelay=1.0.
	// However, it will not put one more data at the end of trajectory, when offsetDealy<1.0 and onsetDelay=0.0.
	// Otherwise, the trajectory is possible to shift one time slide.
	// Here, I want to make sure the size of trajectory is correct. - Johnny 9/4/09
	double len = duration/step + 1; // this should be the size of vector
	while((int)trajectory->X.size() > len)
	{
		if(onsetDelay > 0.0)
		{
			trajectory->X.erase(trajectory->X.begin());
			trajectory->Y.erase(trajectory->Y.begin());
			trajectory->Z.erase(trajectory->Z.begin());
		}
		else
		{	//This should never happen, but for safety, make sure the size is reduce one in while loop.
			trajectory->X.erase(trajectory->X.end());
			trajectory->Y.erase(trajectory->Y.end());
			trajectory->Z.erase(trajectory->Z.end());
		}
	}
}

void MoogDotsCom::CreateWholeTraj(vector<double> *trajectory, double duration, double onsetDelay)
{
	double step = 1/60.0;
	double t = 0.0;
	while(t < duration*onsetDelay)	// add onset delay period data in front
	{
		trajectory->insert(trajectory->begin(),trajectory->at(0));
		t += step;
	}
	t = (double)trajectory->size()*step;
	while(t <= duration) // add offset delay period data at back
	{
		trajectory->push_back(trajectory->back());
		t += step;
	}

	// When we input the onsetDelay=0.2, the program will read to 0.20000000000000001.
	// Then it is possible put one more data in front of trajectory, when onsetDelay>0.0 and offsetDelay=1.0.
	// However, it will not put one more data at the end of trajectory, when offsetDealy<1.0 and onsetDelay=0.0.
	// Otherwise, the trajectory is possible to shift one time slide.
	// Here, I want to make sure the size of trajectory is correct. - Johnny 9/4/09
	double len = duration/step + 1; // this should be the size of vector
	while((int)trajectory->size() > len)
	{
		if(onsetDelay > 0.0)
		{
			trajectory->erase(trajectory->begin());
		}
		else
		{	//This should never happen, but for safety, make sure the size is reduce one in while loop.
			trajectory->erase(trajectory->end());
		}
	}
}

DATA_FRAME MoogDotsCom::FindStartFrame()
{
	nmMovementData tmpData, tmpRotData;

	vector<double> platformCenter = g_pList.GetVectorData("PLATFORM_CENTER"),
		           headCenter = g_pList.GetVectorData("HEAD_CENTER"),
				   origin = g_pList.GetVectorData("M_ORIGIN"),
				   rotationOffsets = g_pList.GetVectorData("ROT_CENTER_OFFSETS"),
				   rotStartOffset = g_pList.GetVectorData("ROT_START_OFFSET");

	// Point is the center of the platform, rotPoint is the subject's head + offsets.
	nm3DDatum point, rotPoint;
	point.x = platformCenter.at(0) + origin.at(0); 
	point.y = platformCenter.at(1) + origin.at(1);
	point.z = platformCenter.at(2) - origin.at(2);
	rotPoint.x = headCenter.at(0)/100.0 + CUBE_ROT_CENTER_X - PLATFORM_ROT_CENTER_X + origin.at(0) + rotationOffsets.at(0)/100.0;
	rotPoint.y = headCenter.at(1)/100.0 + CUBE_ROT_CENTER_Y - PLATFORM_ROT_CENTER_Y + origin.at(1) + rotationOffsets.at(1)/100.0;
	rotPoint.z = headCenter.at(2)/100.0 + CUBE_ROT_CENTER_Z - PLATFORM_ROT_CENTER_Z - origin.at(2) + rotationOffsets.at(2)/100.0;

	// Parameters for the rotation.
	double // We negate elevation to be consistent with previous program conventions.
		   elevation = -g_pList.GetVectorData("ROT_ELEVATION").at(0),
		   azimuth = g_pList.GetVectorData("ROT_AZIMUTH").at(0);

	nmRotatePointAboutPoint(point, rotPoint, elevation, azimuth, &rotStartOffset,
							&tmpData, &tmpRotData, true, true);

	// Flip the sign of the roll because the Moog needs to the roll to be opposite of what
	// the preceding function generates.  We also flip the sign of the heave because the
	// equations assume positive is up, whereas the Moog thinks negative is up.
	for (int i = 0; i < static_cast<int>(tmpRotData.X.size()); i++) {
		// The rexroth system uses radians instead of degrees, so we must convert the
		// output of the rotation calculation.
		tmpRotData.Z.at(i) *= -1.0;			// Roll
		tmpData.Z.at(i) *= -1.0;			// Heave
	}

	DATA_FRAME moogFrame;
	moogFrame.lateral = static_cast<float>(tmpData.X.front());
	moogFrame.surge = static_cast<float>(tmpData.Y.front());
	moogFrame.heave = static_cast<float>(tmpData.Z.front());
	moogFrame.yaw = static_cast<float>(tmpRotData.X.front());
	moogFrame.pitch = static_cast<float>(tmpRotData.Y.front());
	moogFrame.roll = static_cast<float>(tmpRotData.Z.front());

	return moogFrame;
}

DATA_FRAME MoogDotsCom::GetDataFrame()
{
	DATA_FRAME moogFrame;
	GET_DATA_FRAME(&moogFrame);

#if DOF_MODE
	// GET_DATA_FRAME return the Moog's heave axis.
	// We must substrate an offset to that value to adjust for the heave value based around zero
	moogFrame.heave -= MOTION_BASE_CENTER;
#endif

	return moogFrame;
}

int MoogDotsCom::sizeOfInterp()
{
	return (int)m_interpLateral.size();
}

void MoogDotsCom::GoToOrigin()
{
	DATA_FRAME startFrame = FindStartFrame();

	if(motionType==0.0 && g_pList.GetVectorData("GO_TO_ZERO").at(0) == 2.0)
		//g_pList.GetVectorData("M_DIST").at(0)+g_pList.GetVectorData("M_DIST").at(1) != 0.0 ) //no movement on both
	{
		// Gaussian translation
		// Get the positions currently in the command buffer.  We use the thread safe
		// version of GetAxesPositions() here because MovePlatform() is called from
		// both the main GUI thread and the communication thread.
		startFrame = GetDataFrame();
		vector<double>	endPoint = g_pList.GetVectorData("M_ORIGIN"),
						preEle = g_pList.GetVectorData("M_ELEVATION"),
						preAzi = g_pList.GetVectorData("M_AZIMUTH"),
						preMagnitude = g_pList.GetVectorData("M_DIST");
		double elevation, azimuth, magnitude, duration, sigma;
		duration = g_pList.GetVectorData("M_TIME").at(0);
		sigma = g_pList.GetVectorData("M_SIGMA").at(0);

		vector<double> backGLorigin, backAzi, backEle, backMagnitude;
		Vector3 backDir;
		//Find back direction vector for moog
		backDir.set(endPoint.at(0)-startFrame.lateral, //unit is meter
					endPoint.at(1)-startFrame.surge,
					endPoint.at(2)-startFrame.heave);
		magnitude = backDir.magnitude();
		backMagnitude.push_back(magnitude);

		//If magnitude is bigger than TINY_NUMBER, find elevation and azimuth.
		if(fabs(magnitude) > TINY_NUMBER)
		{
			elevation = asin(backDir.z/magnitude)*RAD2DEG;
			azimuth = atan2(backDir.y, backDir.x)*RAD2DEG;
			backEle.push_back(elevation);
			backAzi.push_back(azimuth);
		}
		else
		{
			elevation = preEle.at(0);
			azimuth = preAzi.at(0);
			backAzi.push_back(preEle.at(0)); 
			backEle.push_back(preAzi.at(0));
		}

		GLPanel *glPanel = m_glWindow->GetGLPanel();
		
		//Following is how we set for OpenGL in Compute() function
		//glPanel->SetLateral(m_interpLateral.at(m_grabIndex)*100.0);
		//glPanel->SetHeave(m_interpHeave.at(m_grabIndex)*100.0 + GL_OFFSET);
		//glPanel->SetSurge(m_interpSurge.at(m_grabIndex)*100.0);
		//glPanel->SetAllObjRotAngle(m_grabIndex);

		//M_ORIGIN usually only set for Moog platform. In some situation(M_DIST 0.0 0.1), 
		//moog platform doesn't move, but camera is moved. We need borrow M_ORIGIN parameter to
		//reset the camera starting position to back to starting point.
		//Here, Moog platform will use &startFrame for the start position and will not use value of 
		//M_ORIGIN parameter again. -Johnny 7/25/2011

		//Change currCameraTrans in OpenGL coord to Moog platform coord.
		backGLorigin.push_back(glPanel->currCameraTrans.x/100.0); backGLorigin.push_back(-glPanel->currCameraTrans.z/100.0);
#if DOF_MODE		
		backGLorigin.push_back((-glPanel->currCameraTrans.y-GL_OFFSET)/100.0-MOTION_BASE_CENTER); //same as -glPanel->currCameraTrans.y
#else 
		backGLorigin.push_back((-glPanel->currCameraTrans.y-GL_OFFSET)/100.0);
#endif		
		//Find back direction vector for camera
		backDir.set(endPoint.at(0)-backGLorigin.at(0), //unit is meter
					endPoint.at(1)-backGLorigin.at(1),
					endPoint.at(2)-backGLorigin.at(2));
		magnitude = backDir.magnitude();
		backMagnitude.push_back(magnitude);

		//If magnitude is bigger than TINY_NUMBER, find elevation and azimuth.
		if(fabs(magnitude) > TINY_NUMBER)
		{
			elevation = asin(backDir.z/magnitude)*RAD2DEG;
			azimuth = atan2(backDir.y, backDir.x)*RAD2DEG;
			backEle.push_back(elevation);
			backAzi.push_back(azimuth); 
		}
		else
		{
			backAzi.push_back(preEle.at(1)); 
			backEle.push_back(preAzi.at(1));
		}

		//Reset M_ELEVATION, M_AZIMUTH, M_ORIGIN, and M_DIST 
		//for camera trajactory computation in CalculateGaussianMovement()
		g_pList.SetVectorData("M_ELEVATION", backEle);
		g_pList.SetVectorData("M_AZIMUTH", backAzi);
		g_pList.SetVectorData("M_ORIGIN", backGLorigin); //From current ending point(original) back to start point.
		g_pList.SetVectorData("M_DIST", backMagnitude);

		// Calculate the trajectory to move the motion base into start position and along the vector.
		CalculateGaussianMovement(&startFrame, elevation, azimuth, backMagnitude.at(0), duration, sigma, true);

		// make sure camera back to origin
		m_interpLateral.push_back(m_glData.X.back());
		m_interpSurge.push_back(m_glData.Y.back());
		m_interpHeave.push_back(m_glData.Z.back());
		
		g_pList.SetVectorData("M_ORIGIN", endPoint);
		g_pList.SetVectorData("M_ELEVATION", preEle);
		g_pList.SetVectorData("M_AZIMUTH", preAzi);
		g_pList.SetVectorData("M_DIST", preMagnitude);
	}
	else
	{
		// This will move the motion base from its current position to (0, 0, 0).
		CalculateGaussianMovement(&startFrame, 0.0, 0.0, 0.0, 0.0, 0.0, false);
	}

	// Start the movement.
	ThreadDoCompute(COMPUTE | RECEIVE_COMPUTE);
}