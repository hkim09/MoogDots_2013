#include "StdAfx.h"
#include "MoogDots.h"
#include <stdlib.h>

// Parameter list -- Original declaration can be found in ParameterList.cpp
extern CParameterList g_pList;

// Declare all Global variables here
int RIG_ROOM;
float CENTER2SCREEN;
int SCREEN_WIDTH;  //in pixels, as set by user
int SCREEN_HEIGHT;
int PROJECTOR_NATIVE_WIDTH;		//in pixels
int PROJECTOR_NATIVE_HEIGHT;

// PLATFORM_ROT_CENTER is the center of rotation of Moog relative to the back rear center of Moog base.
// Moog coord: x-positive go right, y-positive go foreward, z-positive go downward, all in meters
// PLATFORM_ROT_CENTER coord preserve "right hand rule", so z-positive go upward.
// Let origin of PLATFORM_ROT_CENTER coord system at the back rear center of Moog base.
double	PLATFORM_ROT_CENTER_X, //the center of roll motion(azi=90,ele=0) (set this to 0)
		PLATFORM_ROT_CENTER_Y, //the center of pitch motion(azi=0,ele=0) (distance from center of pitch motion to the back rear of Moog base)
		PLATFORM_ROT_CENTER_Z; //the center of roll motion(azi=90,ele=0) (it is below the Moog base)
// CUBE_ROT_CENTER specifies the center of the eye-coil cube (for rotation), relative to the back rear center of the MOOG.
//  x-positive go right, y-positive go foreward, z-positive go downward, all in meters
//NOTE: Values are rig-specific and stored in the rig-specific settings
double	CUBE_ROT_CENTER_X, //distance of cube center X to Moog base center X
		CUBE_ROT_CENTER_Y, //distance of cube center Y to the back rear of Moog base
		CUBE_ROT_CENTER_Z; //distance of cube center Z and Moog base platform 
//former variable CENTROID_OFFSET = CUBE_ROT_CENTER - PLATFORM_ROT_CENTER (all unit is meter)
//The global variable PLATFORM_ROT_CENTER and CUBE_ROT_CENTER is the replacement of CENTROID_OFFSET,
//so that we can measure the cube on Moog base and figure out the rotation cention. - Johnny 9/24/09

CMainFrame::CMainFrame(const wxChar *title, int xpos, int ypos, int width, int height) :
			wxFrame((wxFrame *) NULL, -1, title, wxPoint(xpos, ypos), wxSize(width, height)),
			m_isRDXOpen(true), m_moogCom(NULL), m_mainPanel(NULL)
{
	// Setup the menu bar.
	m_menuBar = new wxMenuBar();
	m_fileMenu = new wxMenu();
	m_toolsMenu = new wxMenu();
	m_connectionTypeMenu = new wxMenu();
	m_connectionTypeMenu->AppendCheckItem(MENU_TOOLS_CTYPE_TEMPO, "Tempo", "Tempo Control");
	m_connectionTypeMenu->AppendCheckItem(MENU_TOOLS_CTYPE_SPIKE2, "Pipes", "Pipes Control");
	m_fileMenu->Append(MENU_FILE_EXIT, "Exit", "Exits the Program");
	m_toolsMenu->Append(-1, "Connect", m_connectionTypeMenu, "Chooses the connection type");
	m_toolsMenu->Append(MENU_TOOLS_DISCONNECT, "Disconnect", "Disconnect from the MBC.");
	m_toolsMenu->AppendSeparator();
	m_toolsMenu->Append(MENU_TOOLS_CONNECT_PIPES, "Connect Pipes", "Connect to the Pipes only.");
	m_toolsMenu->Append(MENU_TOOLS_DISCONNECT_PIPES, "Disconnect Pipes", "Disconnect from the Pipes only.");
	m_toolsMenu->AppendSeparator();
	m_toolsMenu->AppendCheckItem(MENU_TOOLS_TEMPO, "Listen Mode", "Toggles MoogDots to listen for external control.");
	m_toolsMenu->AppendCheckItem(MENU_TOOLS_TIMER, "Low Priority Mode", "Allows other OpenGL programs to run.");
	m_toolsMenu->AppendSeparator();
	m_toolsMenu->AppendCheckItem(MENU_TOOLS_VERBOSE, "Verbose Output");
//	m_toolsMenu->AppendCheckItem(MENU_TOOLS_VSYNC, "Enable VSync");
	m_toolsMenu->Append(MENU_SET_PACKET_RATE, "Set Packet Rate");
	m_menuBar->Append(m_fileMenu, "File");
	m_menuBar->Append(m_toolsMenu, "Tools");
	SetMenuBar(m_menuBar);

	// Disable the some of the options.
	m_toolsMenu->Enable(MENU_TOOLS_DISCONNECT, false);
	m_toolsMenu->Enable(MENU_TOOLS_CONNECT_PIPES, false);
	m_toolsMenu->Enable(MENU_TOOLS_DISCONNECT_PIPES, false);
	m_toolsMenu->Enable(MENU_TOOLS_TIMER, false);

#if LISTEN_MODE
	m_toolsMenu->Check(MENU_TOOLS_TEMPO, true);
#endif

#if VERBOSE_MODE
	m_toolsMenu->Check(MENU_TOOLS_VERBOSE, true);
#endif

	// Check the default connection type.
	/*
#if DEF_LISTEN_MODE == 1
	m_connectionTypeMenu->Check(MENU_TOOLS_CTYPE_TEMPO, true);
#elif DEF_LISTEN_MODE == 2
	m_connectionTypeMenu->Check(MENU_TOOLS_CTYPE_SPIKE2, true);
#endif
	*/

	// Create the Moog communications object.
	m_moogCom = new MoogDotsCom(
								this,
#if USE_LOCALHOST
								"127.0.0.1", 991,
								"127.0.0.1", 1978,
#else 
									"128.127.55.120", 991,
									//"128.127.55.121", 1978,
									"128.127.55.121", 992,
#endif							
//#if CUSTOM_TIMER - Johnny 6/17/07
//								true);
//#else
								false);
//#endif

	// We'll let the ReceiveCompute function run the whole time.
	m_moogCom->DoCompute(RECEIVE_COMPUTE);

#if SINGLE_CPU_MACHINE
	m_moogCom->SetComThreadPriority(THREAD_PRIORITY_NORMAL);
#endif

#if USE_MATLAB
	// Start the Matlab engine.
	m_moogCom->StartMatlab();
#endif

	// Create the main panel of the program.
	m_mainPanel = new CMainPanel(this, -1, m_moogCom);

	// Create default status bar to start with.
    CreateStatusBar(1);
    SetStatusText("Welcome to MoogDots!");
    m_statBar = GetStatusBar();
}


CMainFrame::~CMainFrame()
{
	if (m_moogCom) { 
		delete m_moogCom; m_moogCom = NULL;
	}
	if (m_mainPanel) {
		delete m_mainPanel; m_mainPanel = NULL;
	}
}

/***************************************************************/
/*	Event Table												   */
/***************************************************************/
BEGIN_EVENT_TABLE(CMainFrame, wxFrame)
EVT_MENU(MENU_FILE_EXIT, CMainFrame::OnMenuFileExit)
EVT_MENU(MENU_TOOLS_DISCONNECT, CMainFrame::OnMenuToolsDisconnect)
EVT_MENU(MENU_TOOLS_CONNECT_PIPES, CMainFrame::OnMenuToolsConnectPipes)
EVT_MENU(MENU_TOOLS_DISCONNECT_PIPES, CMainFrame::OnMenuToolsDisconnectPipes)
EVT_MENU(MENU_TOOLS_TEMPO, CMainFrame::OnMenuToolsTempo)
EVT_MENU(MENU_TOOLS_TIMER, CMainFrame::OnMenuToolsTimer)
EVT_MENU(MENU_TOOLS_VERBOSE, CMainFrame::OnMenuToolsVerboseMode)
//EVT_MENU(MENU_TOOLS_VSYNC, CMainFrame::OnMenuToolsVSync)
EVT_MENU(MENU_SET_PACKET_RATE, CMainFrame::OnMenuToolsPacketRate)
EVT_MENU(MENU_TOOLS_CTYPE_TEMPO, CMainFrame::OnMenuToolsCTypeTempo)
EVT_MENU(MENU_TOOLS_CTYPE_SPIKE2, CMainFrame::OnMenuToolsCTypeSpike2)
EVT_CLOSE(CMainFrame::OnFrameClose)
END_EVENT_TABLE()


void CMainFrame::OnMenuToolsCTypeTempo(wxCommandEvent &event)
{
	if (m_connectionTypeMenu->IsChecked(MENU_TOOLS_CTYPE_TEMPO) == true) {
		// Uncheck all other items in the menu.
		wxMenuItemList itemList = m_connectionTypeMenu->GetMenuItems();
		for (wxMenuItemList::Node *node = itemList.GetFirst(); node; node = node->GetNext()) {
			wxMenuItem *mi = (wxMenuItem*)node->GetData();
			if (mi->GetId() != MENU_TOOLS_CTYPE_TEMPO) {
				mi->Check(false);
			}
		}

		m_moogCom->SetConnectionType(MoogDotsCom::ConnectionType::Tempo);
	}

	ConnectMBC();
}


void CMainFrame::OnMenuToolsCTypeSpike2(wxCommandEvent &event)
{
	if (m_connectionTypeMenu->IsChecked(MENU_TOOLS_CTYPE_SPIKE2) == true) {
		// Uncheck all other items in the menu.
		wxMenuItemList itemList = m_connectionTypeMenu->GetMenuItems();
		for (wxMenuItemList::Node *node = itemList.GetFirst(); node; node = node->GetNext()) {
			wxMenuItem *mi = (wxMenuItem*)node->GetData();
			if (mi->GetId() != MENU_TOOLS_CTYPE_SPIKE2) {
				mi->Check(false);
			}
		}

		m_moogCom->SetConnectionType(MoogDotsCom::ConnectionType::Pipes);
	}

	ConnectMBC();
}


void CMainFrame::OnMenuToolsVSync(wxCommandEvent &event)
{
	if (m_toolsMenu->IsChecked(MENU_TOOLS_VSYNC)) {
		m_moogCom->SetVSyncState(true);
		m_moogCom->UseCustomTimer(true);
	}
	else {
		m_moogCom->SetVSyncState(false);
		m_moogCom->UseCustomTimer(false);
	}
}

void CMainFrame::OnMenuToolsPacketRate(wxCommandEvent &event)
{
	// Create and show a dialog with the current packet rate.
	wxTextEntryDialog dlg(this, "Packet Rate (ms)", "Packet Rate", wxString::Format("%f", m_moogCom->GetPacketRate()));
	int ok = dlg.ShowModal();

	// If the use presses OK, then set the packet rate.
	if (ok == wxID_OK) {
		// Grab the value entered, convert it to a double, and set the packet rate
		// for the thread.
		wxString value = dlg.GetValue();
		double cval;
		if (value.ToDouble(&cval) == true) {
			// Make sure that the value is within an acceptable range.
			if (cval < 15.0 || cval > 18) {
				wxMessageDialog d(this, "Invalid range. Must be [15, 18].", "Range Error", wxOK | wxICON_ERROR);
				d.ShowModal();
			}
			else {
				m_moogCom->SetPacketRate(cval);
			}
		}
		else {
			// Complain
			wxMessageDialog d(this, "Failure to convert string to double.", "Conversion Error", wxOK | wxICON_ERROR);
			d.ShowModal();
		}
	}
}


void CMainFrame::OnMenuToolsVerboseMode(wxCommandEvent &event)
{
	if (m_toolsMenu->IsChecked(MENU_TOOLS_VERBOSE)) {
		m_moogCom->SetVerbosity(true);
	}
	else {
		m_moogCom->SetVerbosity(false);
	}
}


void CMainFrame::OnFrameClose(wxCloseEvent &event)
{
	// Disconnect from the MBC.
	m_moogCom->Disconnect();

#if USE_MATLAB
	// Kill Matlab.
	m_moogCom->CloseMatlab();
#endif
	
	Destroy();

	if (m_moogCom) { 
		delete m_moogCom; m_moogCom = NULL;
	}
	if (m_mainPanel) {
		delete m_mainPanel; m_mainPanel = NULL;
	}
}


void CMainFrame::OnMenuToolsTimer(wxCommandEvent &event)
{
	if (m_toolsMenu->IsChecked(MENU_TOOLS_TIMER)) {
		// Use the built-in timer, lower the thread and process priority, and hide the OpenGL window.
		m_moogCom->ListenMode(false);

//#if CUSTOM_TIMER - Johnny 6/17/07
//		if (m_toolsMenu->IsChecked(MENU_TOOLS_VSYNC))
//			m_moogCom->UseCustomTimer(false);
//#endif
		SetPriorityClass(GetCurrentProcess(), NORMAL_PRIORITY_CLASS);
		m_moogCom->SetComThreadPriority(THREAD_PRIORITY_NORMAL);

		m_moogCom->ShowGLWindow(false);

		// Close the Tempo port.
		if (m_isRDXOpen) {
			m_moogCom->CloseTempo();
			m_isRDXOpen = false;
		}
	}
	else {
		// Switch to the custom timer, kick the thread and process priority up, and display the
		// OpenGL window.
		SetPriorityClass(GetCurrentProcess(), REALTIME_PRIORITY_CLASS);
		m_moogCom->SetComThreadPriority(THREAD_PRIORITY_TIME_CRITICAL);

		m_moogCom->ShowGLWindow(true);
//#if CUSTOM_TIMER - Johnny 6/17/07
//		if (m_toolsMenu->IsChecked(MENU_TOOLS_VSYNC))
//			m_moogCom->UseCustomTimer(true);
//#endif

		// Open the Tempo port.
		if (m_isRDXOpen == false) {
			m_moogCom->OpenTempo();
			m_isRDXOpen = true;
		}

		if (m_toolsMenu->IsChecked(MENU_TOOLS_TEMPO)) {
			m_moogCom->ListenMode(true);
		}
	}
}


void CMainFrame::OnMenuToolsTempo(wxCommandEvent &event)
{
	// Disable manual controls if Tempo mode is toggled.
	if (m_toolsMenu->IsChecked(MENU_TOOLS_TEMPO)) {
		m_mainPanel->Enable(FALSE);

		if (m_toolsMenu->IsChecked(MENU_TOOLS_TIMER) == false) {
			m_moogCom->ListenMode(true);
		}
	}
	else {
		m_mainPanel->Enable(TRUE);
		m_moogCom->ListenMode(false);
	}
}


void CMainFrame::ConnectMBC()
{
	bool errorOccurred = true;
	wxString errorString = "";
	int errorCode,
		retCode;

	m_toolsMenu->Enable(MENU_TOOLS_CTYPE_TEMPO, false);
	m_toolsMenu->Enable(MENU_TOOLS_CTYPE_SPIKE2, false);

	// Initialize the pipes crap if selected.
	if (m_moogCom->GetConnectionType() == MoogDotsCom::ConnectionType::Pipes) {
		m_moogCom->InitPipes();
		m_toolsMenu->Enable(MENU_TOOLS_DISCONNECT_PIPES, true);
	}

	// Check to make sure that a connection type was selected.  If a connection
	// type was selected, then try to connect.
	if (m_moogCom->GetConnectionType() == MoogDotsCom::ConnectionType::None) {
		retCode = 3;
	}
	else {
		retCode = m_moogCom->Connect(errorCode);
	}

	switch (retCode)
	{
		// Success
	case 0:
		// Turn off the Connect option and enable the
		// Disconnect option.
		m_toolsMenu->Enable(MENU_TOOLS_DISCONNECT, true);

		// Turn on the built-in timer option.
		m_toolsMenu->Enable(MENU_TOOLS_TIMER, true);

		errorOccurred = false;
		break;

		// Error occurred
	case -1:
		errorString = wxString::Format("Connect Error: %d", errorCode);
		break;

		// Alread connected
	case 1:
		errorString = "Already Connected";
		break;

		// No connection type was selected.
	case 3:
		errorString = "Please select a connection type";
		break;
	}

	if (errorOccurred == true) {
		wxMessageDialog d(this, errorString);
		d.ShowModal();
	}
}

void CMainFrame::OnMenuToolsDisconnect(wxCommandEvent &event)
{
	// Turn off the Disconnect option and enable the
	// Connect option.
	m_toolsMenu->Enable(MENU_TOOLS_DISCONNECT, false);
	m_toolsMenu->Enable(MENU_TOOLS_CONNECT_PIPES, false);
	m_toolsMenu->Enable(MENU_TOOLS_DISCONNECT_PIPES, false);

	// Uncheck all other items in the connect menu and enable it.
	wxMenuItemList itemList = m_connectionTypeMenu->GetMenuItems();
	for (wxMenuItemList::Node *node = itemList.GetFirst(); node; node = node->GetNext()){
		node->GetData()->Check(false);
		node->GetData()->Enable(true);
	}

	// Uncheck and disable the built-in timer option.
	m_toolsMenu->Check(MENU_TOOLS_TIMER, false);
	m_toolsMenu->Enable(MENU_TOOLS_TIMER, false);


//#if CUSTOM_TIMER - Johnny
	// Switch to the custom timer, kick the thread and process priority up, and display the
	// OpenGL window.
	//if (m_toolsMenu->IsChecked(MENU_TOOLS_VSYNC)) {
	if(m_moogCom->m_customTimer){
		SetPriorityClass(GetCurrentProcess(), REALTIME_PRIORITY_CLASS);
		m_moogCom->SetComThreadPriority(THREAD_PRIORITY_TIME_CRITICAL);
		m_moogCom->UseCustomTimer(true);
		m_moogCom->ShowGLWindow(true);
	}
//#endif

	// Disconnect
	m_moogCom->Disconnect();

	// Close the pipes.
	if (m_moogCom->GetConnectionType() == MoogDotsCom::ConnectionType::Pipes) {
		Sleep(50);
		m_moogCom->ClosePipes();
	}
}

void CMainFrame::OnMenuToolsConnectPipes(wxCommandEvent &event)
{
	// Initialize the pipes crap if selected.
	if (m_moogCom->GetConnectionType() == MoogDotsCom::ConnectionType::Pipes) {
		m_moogCom->InitPipes();
		m_moogCom->ConnectPipes();
		m_toolsMenu->Enable(MENU_TOOLS_CONNECT_PIPES, false);
		m_toolsMenu->Enable(MENU_TOOLS_DISCONNECT_PIPES, true);
	}
}

void CMainFrame::OnMenuToolsDisconnectPipes(wxCommandEvent &event)
{
	// Turn off the Disconnect option and enable the
	// Connect option.
	m_toolsMenu->Enable(MENU_TOOLS_CONNECT_PIPES, true);
	m_toolsMenu->Enable(MENU_TOOLS_DISCONNECT_PIPES, false);

	// Close the pipes.
	if (m_moogCom->GetConnectionType() == MoogDotsCom::ConnectionType::Pipes) {
		Sleep(50);
		m_moogCom->ClosePipes();
	}

	// if checked listening mode, then uncheck it
	if (m_toolsMenu->IsChecked(MENU_TOOLS_TEMPO)) {
		m_toolsMenu->Check(MENU_TOOLS_TEMPO, false);
		OnMenuToolsTempo(event);
	}
}

void CMainFrame::OnMenuFileExit(wxCommandEvent &event)
{
	// Disconnect from the MBC.
	m_moogCom->Disconnect();

#if USE_MATLAB
	// Kill Matlab.
	m_moogCom->CloseMatlab();
#endif

	Destroy();

	delete m_moogCom; m_moogCom = NULL;
	delete m_mainPanel; m_mainPanel = NULL;
}


/*******************************************************************************/
/*	Create the Application.													   */
/*******************************************************************************/
IMPLEMENT_APP(MoogDots)

bool MoogDots::OnInit()
{
	// Initial all global variables accroding to the Moog system
	InitRig();

	//Chris original program
	int winLocX, winLocY = 250;

#if DUAL_MONITORS
#if FLIP_MONITORS
	winLocX = 250;
#else
	winLocX = 250 + SCREEN_WIDTH;
#endif
#else
	winLocX = 7;
#endif

	// Create the main window.
#if USE_LOCALHOST
	CMainFrame *m_mainFrame = new CMainFrame("MoogDots (Localhost)", winLocX, winLocY, 435, 450);
#else
	CMainFrame *m_mainFrame = new CMainFrame("MoogDots", winLocX, winLocY, 435, 450);
#endif

	m_mainFrame->SetIcon(wxIcon("MAIN_ICO"));
	m_mainFrame->Show(true);
	SetTopWindow(m_mainFrame);

#if SINGLE_CPU_MACHINE
	SetPriorityClass(GetCurrentProcess(), NORMAL_PRIORITY_CLASS);
#else
	SetPriorityClass(GetCurrentProcess(), REALTIME_PRIORITY_CLASS);
#endif

	return true;
}

void MoogDots::InitRig()
{
	// We need setup different rig for different Moog system in different labs.
	// so we setup all global variables that relate to rig in here.
	// Read the C:\rig_id.txt file before create the application.
	wxTextFile rigFile(RIG_FILE);
	wxString str;

	if(rigFile.Exists())
	{
		rigFile.Open(RIG_FILE);
		str = rigFile.GetFirstLine();
		if (str == "MOOG_164") RIG_ROOM = MOOG_164;
		else if(str == "MOOG_157") RIG_ROOM = MOOG_157;
		else
		{
			wxMessageDialog d(NULL, "C:\\rig_id.txt file has error. Please delete it and restart the program.", 
				"Error", wxICON_ERROR);
			d.ShowModal();
			::exit(0);
		}
	}
	else // the rig_id.txt file does not exit
	{
		rigFile.Create(RIG_FILE);
		rigFile.Clear();
		rigFile.AddLine("MOOG_164");
		rigFile.Write();
		RIG_ROOM = MOOG_164;
		wxString str = "C:\\rig_id.txt file doesn't exist! Now we created it.\n";
			str += "By default, we set MOOG_164 for Rig control.";
		wxMessageDialog d(NULL, str, "Information",wxICON_INFORMATION);
		d.ShowModal();
	}
	rigFile.Close();

	vector<double> pred_offset, frame_delay;
	//define Global variables here
	switch ( RIG_ROOM )
	{
		case MOOG_164:
			CENTER2SCREEN = 38;
			//SCREEN_WIDTH = 1280;
			//SCREEN_HEIGHT = 1024;
			PROJECTOR_NATIVE_WIDTH = 1400;
			PROJECTOR_NATIVE_HEIGHT = 1050;
			pred_offset.push_back(517.0);
			frame_delay.push_back(17.0);
			PLATFORM_ROT_CENTER_X = 0.0;
			PLATFORM_ROT_CENTER_Y = 0.568;
			PLATFORM_ROT_CENTER_Z = -0.115;
			CUBE_ROT_CENTER_X = 0.0;
			CUBE_ROT_CENTER_Y = 0.453;
			CUBE_ROT_CENTER_Z = 0.775; 
			break;
		case MOOG_157:
			CENTER2SCREEN = 38;
			//SCREEN_WIDTH = 1280;
			//SCREEN_HEIGHT = 1024;
			PROJECTOR_NATIVE_WIDTH = 1400;
			PROJECTOR_NATIVE_HEIGHT = 1050;
			pred_offset.push_back(491.0);
			frame_delay.push_back(17.0);
			PLATFORM_ROT_CENTER_X = 0.0;
			PLATFORM_ROT_CENTER_Y = 0.5645;
			PLATFORM_ROT_CENTER_Z = -0.111;
			CUBE_ROT_CENTER_X = -0.0025;
			CUBE_ROT_CENTER_Y = 0.38;
			CUBE_ROT_CENTER_Z = 0.774; 
			break;
		default:
			break;
	}
	g_pList.SetVectorData("PRED_OFFSET", pred_offset);
	g_pList.SetVectorData("FRAME_DELAY", frame_delay);

	//Find the primary display monitor width and height
	SCREEN_WIDTH = GetSystemMetrics(SM_CXSCREEN);
	SCREEN_HEIGHT = GetSystemMetrics(SM_CYSCREEN);
}
