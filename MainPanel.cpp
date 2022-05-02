#include "StdAfx.h"
#include "MainPanel.h"
#include "MoogDots.h"

// Parameter list -- Original declaration can be found in ParameterList.cpp
extern CParameterList g_pList;
extern int RIG_ROOM;
#if RECORD_MOVIE
	extern CGLToMovie g_MovieRecorder;
#endif

CMainPanel::CMainPanel(/*wxWindow*/CMainFrame *parent, wxWindowID id, MoogDotsCom *com) :
				 wxPanel(parent, id), m_moogCom(com)
{
	m_mainFrame = parent;
	parent->GetClientSize(&m_width, &m_height);

	// Create all the sizer for the main panel.
	m_topSizer = new wxBoxSizer(wxVERTICAL);
	m_upperSizer = new wxBoxSizer(wxHORIZONTAL);
	m_upperRightSizer = new wxBoxSizer(wxVERTICAL);
	m_parameterSizer = new wxBoxSizer(wxHORIZONTAL);
	m_otherButtonsSizer = new wxBoxSizer(wxVERTICAL);

	m_generalBox = new wxStaticBox(this, -1, "General Controls", wxDefaultPosition, wxSize(m_width-10, 52));
	m_buttonSizer = new wxStaticBoxSizer(m_generalBox, wxHORIZONTAL);

	// Create all general buttons.
	initButtons();
	const int MoogTotal = 2;
	wxString choices[MoogTotal] = {"Moog 164","Moog 157"};
	m_radioBox = new wxRadioBox(this, RIG_RADIO_BOX, "Rig Control", wxDefaultPosition, wxDefaultSize, MoogTotal, choices);
	m_radioBox->SetSelection(RIG_ROOM);

	// Setup Rig Control radio box


	// Create all the parameter list stuff.
	initParameterListStuff();

	// Create the message console.
	m_messageConsole = new wxListBox(this, -1, wxPoint(10, 185), wxSize(m_width-20, 100), 0, NULL, wxLB_HSCROLL);
	m_upperRightSizer->Add(m_messageConsole, 1, wxGROW | wxALL, 5);

	m_upperSizer->Add(m_moogListBox, 0, wxGROW | wxALL, 5);
	m_upperSizer->Add(m_upperRightSizer, 1, wxGROW);
	m_topSizer->Add(m_upperSizer, 1, wxGROW);
	m_topSizer->Add(m_buttonSizer, 0, wxGROW | wxLEFT | wxBOTTOM | wxRIGHT | wxALIGN_BOTTOM, 5);
	m_topSizer->Add(m_radioBox, 0, wxGROW | wxLEFT | wxBOTTOM | wxRIGHT | wxALIGN_BOTTOM, 5);

	// Tell the Moog communication class where the message console is, then initialize the Tempo crap.
	m_moogCom->SetConsolePointer(m_messageConsole);
	m_moogCom->InitTempo();

	SetSizer(m_topSizer);

#if RECORD_MOVIE
	m_recordingMovie = false;
#endif
}

void CMainPanel::initParameterListStuff()
{
	wxString *choices;
	string *keyList;
	int i, keyCount = 0;

	// Grabs the parameter list keys and puts them in an array of strings.  This allows us to display them
	// easily.
	keyList = g_pList.GetKeyList(keyCount);
	wxASSERT(keyCount);
	choices = new wxString[keyCount];
	for (i = 0; i < keyCount; i++) {
		choices[i] = keyList[i].c_str();
	}

	// Create the listbox that shows all of our parameters.
	m_moogListBox = new wxListBox(this, MOOG_LISTBOX, wxDefaultPosition, wxSize(135, 400),
								  g_pList.GetListSize(), choices, wxLB_SINGLE | wxLB_NEEDED_SB | wxLB_SORT | wxLB_HSCROLL);
	m_moogListBox->SetFirstItem(0);  // Sets the first item to be selected.

	// Create the static text that will display the parameter's description.
	m_descriptionText = new wxStaticText(this, -1, "", wxDefaultPosition, wxDefaultSize, wxALIGN_LEFT);
	m_upperRightSizer->Add(m_descriptionText, 0, wxGROW | wxALL, 5);

	// Create the text box that will display a parameter's corresponding data.
	m_dataTextBox = new wxTextCtrl(this, -1, "", wxDefaultPosition, wxSize(150, 100), wxALIGN_LEFT | wxTE_MULTILINE);
	m_parameterSizer->Add(m_dataTextBox, 1, wxGROW);
	m_parameterSizer->Add(m_otherButtonsSizer, 0, wxGROW | wxLEFT, 5);
	m_upperRightSizer->Add(m_parameterSizer, 0, wxGROW | wxALL, 5);

	// This makes sure that the information for the 1st item in the list shows up.
	OnItemSelected(wxCommandEvent(NULL));

	// release memory
	delete [] keyList; keyList = NULL;
	delete [] choices; choices = NULL;
}

void CMainPanel::initButtons()
{
	int buttonWidth = 75,
		buttonHeight = 31;

	// General control buttons.
	m_EngageButton = new wxButton(this, ENGAGE_BUTTON, "Engage", wxDefaultPosition, wxSize(buttonWidth, buttonHeight));
	m_EngageButton->SetToolTip("Engages the Motion Base");
	m_StopButton = new wxButton(this, STOP_BUTTON, "Stop", wxDefaultPosition, wxSize(buttonWidth, buttonHeight));
	m_StopButton->SetToolTip("Stops All Movement");
	m_ParkButton = new wxButton(this, PARK_BUTTON, "Park", wxDefaultPosition, wxSize(buttonWidth, buttonHeight));
	m_ParkButton->SetToolTip("Parks the Motion Base");
	m_ResetButton = new wxButton(this, RESET_BUTTON, "Reset", wxDefaultPosition, wxSize(buttonWidth, buttonHeight));
	m_ResetButton->SetToolTip("Resets the MBC");

	// Create the Go button.
	m_goButton = new wxButton(this, MOOG_GO_BUTTON, "Go!", wxPoint(305, 145-29), wxSize(75, 30));

	// Create the Set button.
	m_setButton = new wxButton(this, MOOG_SET_BUTTON, "Set Item", wxPoint(305, 45), wxSize(75, 30));

	// Create the Go to Zero button.
	m_goToZeroButton = new wxButton(this, MOOG_ZERO_BUTTON, "Go to Origin", wxPoint(305, 80), wxSize(75, 30));

	// Add the buttons to the sizers.
	m_buttonSizer->Add(2, 0);
	m_buttonSizer->Add(m_EngageButton, 0);
	m_buttonSizer->Add(1, 1, 1);
	m_buttonSizer->Add(m_ParkButton, 0);
	m_buttonSizer->Add(1, 1, 1);
	m_buttonSizer->Add(m_ResetButton, 0);
	m_buttonSizer->Add(1, 1, 1);
	m_buttonSizer->Add(m_StopButton, 0);

	m_otherButtonsSizer->Add(m_setButton, 0);
	m_otherButtonsSizer->Add(1, 1, 1);
	m_otherButtonsSizer->Add(m_goToZeroButton, 0);
	m_otherButtonsSizer->Add(1, 1, 1);
	m_otherButtonsSizer->Add(m_goButton, 0);
}


/***************************************************************/
/*	Event Table												   */
/***************************************************************/
BEGIN_EVENT_TABLE(CMainPanel, wxPanel)
EVT_BUTTON(PARK_BUTTON, CMainPanel::OnParkButtonClicked)
EVT_BUTTON(RESET_BUTTON, CMainPanel::OnResetButtonClicked)
EVT_BUTTON(ENGAGE_BUTTON, CMainPanel::OnEngageButtonClicked)
EVT_LISTBOX(MOOG_LISTBOX, CMainPanel::OnItemSelected)
EVT_BUTTON(MOOG_GO_BUTTON, CMainPanel::OnGoButtonClicked)
EVT_BUTTON(MOOG_SET_BUTTON, CMainPanel::OnSetButtonClicked)
EVT_BUTTON(STOP_BUTTON, CMainPanel::OnStopButtonClicked)
EVT_BUTTON(MOOG_ZERO_BUTTON, CMainPanel::OnGoToZeroButtonClicked)
EVT_RADIOBOX(RIG_RADIO_BOX, CMainPanel::OnRigSelected)
END_EVENT_TABLE()

void CMainPanel::OnGoToZeroButtonClicked(wxCommandEvent &event)
{
	vector<double> tmpVector;
	tmpVector.push_back(2.0);
	g_pList.SetVectorData("GO_TO_ZERO", tmpVector); // go to origin with camera movement

	m_moogCom->GoToOrigin();

	tmpVector.at(0) = 0.0;
	g_pList.SetVectorData("GO_TO_ZERO", tmpVector);

	/*
	vector<double> rotOrigin = g_pList.GetVectorData("ROT_ORIGIN");
	vector<double> transOrigin = g_pList.GetVectorData("M_ORIGIN");

	DATA_FRAME startFrame = m_moogCom->FindStartFrame();
	//startFrame.lateral = 0.0f; startFrame.surge = 0.0f; startFrame.heave = 0.0f;
	//startFrame.yaw = 0.0f; startFrame.pitch = 0.0f; startFrame.roll = 0.0f;
	// Replace by FindStartFrame() that include M_ORIGIN and ROT_SATART_OFFSET 
	//startFrame.lateral = transOrigin.at(0);
	//startFrame.surge = transOrigin.at(1);
	//startFrame.heave = transOrigin.at(2);
	//startFrame.yaw = rotOrigin.at(0);
	//startFrame.pitch = rotOrigin.at(1);
	//startFrame.roll = rotOrigin.at(2);


	//test for gaussion moving back with opengl drawing.
	if(m_moogCom->motionType==0.0)
	{
		// Gaussian translation
		// Get the positions currently in the command buffer.  We use the thread safe
		// version of GetAxesPositions() here because MovePlatform() is called from
		// both the main GUI thread and the communication thread.
		startFrame = m_moogCom->GetDataFrame();
		vector<double> startPoint = g_pList.GetVectorData("M_ORIGIN");
		double elevation, azimuth, magnitude, duration, sigma;
		//elevation = g_pList.GetVectorData("M_ELEVATION").at(0);
		//azimuth = g_pList.GetVectorData("M_AZIMUTH").at(0);
		//magnitude = g_pList.GetVectorData("M_DIST").at(0);
		duration = g_pList.GetVectorData("M_TIME").at(0);
		sigma = g_pList.GetVectorData("M_SIGMA").at(0);

		vector<double> tmpVector;
		Vector3 backDir(startPoint.at(0)-startFrame.lateral,
						startPoint.at(1)-startFrame.surge,
						startPoint.at(2)-startFrame.heave);

		magnitude = backDir.magnitude();
		elevation = asin(backDir.z/magnitude)*RAD2DEG;
		azimuth = atan2(backDir.y, backDir.x)*RAD2DEG;

		tmpVector.push_back(elevation); tmpVector.push_back(elevation);
		g_pList.SetVectorData("M_ELEVATION", tmpVector);
		tmpVector.at(0)=azimuth; tmpVector.at(1)=azimuth;
		g_pList.SetVectorData("M_AZIMUTH", tmpVector);

		//tmpVector.at(0) = 0.5; tmpVector.at(1) = 0.5;
		//g_pList.SetVectorData("M_TIME", tmpVector);
		
		// Calculate the trajectory to move the motion base into start position and along the vector.
		m_moogCom->CalculateGaussianMovement(&startFrame, elevation, azimuth, magnitude, duration, sigma, true);
	}
	else
	{
		// This will move the motion base from its current position to (0, 0, 0).
		m_moogCom->CalculateGaussianMovement(&startFrame, 0.0, 0.0, 0.0, 0.0, 0.0, false);
	}

	// Start the movement.
	m_moogCom->DoCompute(COMPUTE | RECEIVE_COMPUTE);
	*/
}


void CMainPanel::OnEngageButtonClicked(wxCommandEvent &event)
{
	bool errorOccurred = false;
	wxString errorString = "";

	switch (m_moogCom->Engage())
	{
	case 0:		// Success
		break;
	case -1:	// Already engaged
		errorOccurred = true;
		errorString = "Already Engaged";
		break;
	case 1:		// Haven't connected yet
		errorOccurred = true;
		errorString = "Connect to Moog First";
		break;
	}

	if (errorOccurred == true) {
		wxMessageDialog d(this, errorString);
		d.ShowModal();
	}
}


void CMainPanel::OnResetButtonClicked(wxCommandEvent &event)
{
	m_moogCom->Reset();
}


void CMainPanel::OnParkButtonClicked(wxCommandEvent &event)
{
	m_moogCom->Park();
}


void CMainPanel::OnStopButtonClicked(wxCommandEvent &event)
{
	// Stop the Compute() function but let ReceiveCompute() continue.
	m_moogCom->DoCompute(RECEIVE_COMPUTE);

#if USE_MATLAB
	m_moogCom->StuffMatlab();
#endif;

#if RECORD_MOVIE
	m_moogCom->recordMovie = false;
#endif
}


void CMainPanel::OnSetButtonClicked(wxCommandEvent &event)
{
	vector<double> value;
	int i;

	// Make sure that the correct number of data values is being entered for the
	// selected parameter.
	int sizeOfVector = static_cast<int>(g_pList.GetVectorData(m_moogListBox->GetStringSelection().c_str()).size());
	if (m_dataTextBox->GetNumberOfLines() != sizeOfVector) {
		wxMessageDialog d(this, wxString::Format("Parameter \"%s\" takes %d data values.", m_moogListBox->GetStringSelection(), sizeOfVector),
						  "Error", wxICON_ERROR);
		d.ShowModal();
		return;
	}

	// Grab the data from the data box and stuff it into a vector.
	for (i = 0; i < m_dataTextBox->GetNumberOfLines(); i++) {
		double d;

		// This converts the wxString to a double number.
		m_dataTextBox->GetLineText(i).ToDouble(&d);

		// Put the converted string into the vector.
		value.push_back(d);
	}

	// Set the vector data associated with the selected key.
	g_pList.SetVectorData(m_moogListBox->GetStringSelection().c_str(), value);

	// initial rendering
	//if(m_moogCom->sizeOfInterp() > 0)
		m_moogCom->InitRender();

	// Only when DO_MOVEMENT == -1.0, it will calculate starfield.
	//vector<double> tmp;
	//tmp.push_back(-1.0);
	//g_pList.SetVectorData("DO_MOVEMENT", tmp);
	m_moogCom->m_glWindow->GetGLPanel()->SetComputeStarField(true);

	// Update the Frustum and StarField data for the GL scene and re-render it.
	m_moogCom->UpdateGLScene(true);

#if RECORD_MOVIE
	if(!m_recordingMovie)
		g_MovieRecorder.SetMovieDim(g_pList.GetVectorData("MOVIE_DIM").at(0),g_pList.GetVectorData("MOVIE_DIM").at(1));
#endif
}

void CMainPanel::OnGoButtonClicked(wxCommandEvent &event)
{
	// Make sure that the Moog isn't moving.
	m_moogCom->DoCompute(RECEIVE_COMPUTE);

	Sleep(50);

#if CIRCLE_TEST
	m_moogCom->CalculateCircleMovement();
#else
	// Grab the Gaussian parameters.
	vector<double> startPoint;
	double elevation, azimuth, magnitude, duration, sigma;

	// Get the motion type.
	m_moogCom->motionType = static_cast<int>(g_pList.GetVectorData("MOTION_TYPE").at(0));
	m_moogCom->objMotionType = g_pList.GetVectorData("OBJ_MOTION_TYPE");

	switch (m_moogCom->motionType)
	{
	// Gaussian translation
	case 0:
		// Grab the Gaussian parameters.
		startPoint = g_pList.GetVectorData("M_ORIGIN");
		elevation = g_pList.GetVectorData("M_ELEVATION").at(0);
		azimuth = g_pList.GetVectorData("M_AZIMUTH").at(0);
		magnitude = g_pList.GetVectorData("M_DIST").at(0);
		duration = g_pList.GetVectorData("M_TIME").at(0);
		sigma = g_pList.GetVectorData("M_SIGMA").at(0);

		// Store the start point.
		DATA_FRAME startFrame;
		startFrame.lateral = startPoint.at(0);
		startFrame.surge = startPoint.at(1);
		startFrame.heave = startPoint.at(2);
		startFrame.yaw = 0.0f;
		startFrame.pitch = 0.0f;
		startFrame.roll = 0.0f;

		// Calculate the trajectory to move the motion base into start position and along the vector.
		m_moogCom->CalculateGaussianMovement(&startFrame, elevation, azimuth, magnitude, duration, sigma, true);

		// Generate the analog output for the velocity if needed.
		if (g_pList.GetVectorData("OUTPUT_VELOCITY").at(0)) {
			m_moogCom->CalculateStimulusOutput();
		}
		break;

	// Rotation movement
	case 1:
		m_moogCom->CalculateRotationMovement();
		break;

	// Sinusoid movement
	case 2:
		m_moogCom->CalculateSinusoidMovement();
		break;

	// Interval movement.
	case 3:
		m_moogCom->Calculate2IntervalMovement();
		break;

	// Tilt Translation movement.
	case 4:
		m_moogCom->CalculateTiltTranslationMovement();
		break;
	// Gabor movement.
	case 5:
		m_moogCom->CalculateGaborMovement();
		break;
	// Step velocity movement.
	case 6:
		m_moogCom->CalculateStepVelocityMovement();
		break;
	// Motion Parallax movement(sin movement)
	case 7:
		m_moogCom->m_glWindow->GetGLPanel()->SetStimParams();
		m_moogCom->CalculateSinMovement();
		break;
	// Trapezoid velocity movement.
	case 8:
		m_moogCom->CalculateTrapezoidVelocityMovement();
		break;
	// Translation plus rotation movement
	case 9:
		m_moogCom->CalculateTranPlusRotMovement();
		break;
	// Trapezoid Translation plus rotation movement
	case 10:
		m_moogCom->CalculateTranPlusRotMovement(MODE_TRAPEZOID);
		break;
	}

	// Object motion
	for(int i=0; i<NUM_OF_DYN_OBJ; i++)
	{
		switch ((int)m_moogCom->objMotionType.at(i))
		{
		// Gaussian translation
		case 0:
			m_moogCom->CalculateGaussianObjectMovement(i);
			break;
		// Sin(Motion Parallax) object movement
		case 1:
			m_moogCom->CalculateSinObjectMovement(i);
			break;
		// Pseudo depth object movement
		case 2:
			m_moogCom->CalculatePseudoDepthObjectMovement(i);
			break;
		case 3:
			m_moogCom->CalculateConstSpeedObjectMovement(i);
			break;
		case 4:
			m_moogCom->CalculateConstSpeedObjectMovement(i);
			break;
		case 5:
			m_moogCom->CalculateTrapezoidVelocityObjectMovement(i);
			break;
		}
	}

	//When star feild or object start moving, we will change the rendering context,
	// wglMakeCurrent((HDC)m_moogCom->glPanel->GetContext()->GetHDC(), m_threadGLContext);
	// SwapBuffers((HDC)m_moogCom->glPanel->GetContext()->GetHDC());
	//so, we need redo all the glCallList or texture stuff after wglMakeCurrent().
	m_moogCom->m_glWindow->GetGLPanel()->redrawCallList = true;
	m_moogCom->m_glWindow->GetGLPanel()->redrawObjTexture = true;

	//Initial the fixation position, camera postion, rotation center, ect and send cbAOut()
	m_moogCom->InitRender();
#endif

#if RECORD_MOVIE
	m_moogCom->recordMovie = true;
	m_recordingMovie = true;
#endif

	// Start the movement.
	m_moogCom->DoCompute(COMPUTE | RECEIVE_COMPUTE);
}


bool CMainPanel::Enable(bool enable)
{
	bool sumpinChanged = m_goButton->Enable(enable) | m_setButton->Enable(enable) | m_goToZeroButton->Enable(enable);

	return sumpinChanged;
}

void CMainPanel::OnItemSelected(wxCommandEvent &event)
{
	vector<double> value;

	// Set the description for the parameter data.
	m_descriptionText->SetLabel(g_pList.GetParamDescription(m_moogListBox->GetStringSelection().c_str()).c_str());

	// Grab the vector associated with the selected key.
	value = g_pList.GetVectorData(m_moogListBox->GetStringSelection().c_str());

	// If value is empty, then this function is being called from the initialization of the program.  We
	// return so that we don't throw an exception later.
	if (value.empty()) {
		return;
	}

	// Clear the data text box before we write to it.
	m_dataTextBox->Clear();

	// Write each value of the value vector to the data text box, each on its own line.
	// I write it out all weird so that it is formatted nicely in the box.
	int size = static_cast<int>(value.size());
	for (int i = 0; i < size - 1; i++) {
		m_dataTextBox->AppendText(wxString::Format("%f\n", value[i]));
	}
	wxString dataText = wxString::Format("%f", value.at(size-1));
	m_dataTextBox->AppendText(dataText);
}

void CMainPanel::OnRigSelected(wxCommandEvent &event)
{
	int room = m_radioBox->GetSelection();
	wxString str = "";

	str += "Selecting different Moog system will close whole program.\n";
	str += "After you restart the program, the Moog system will be selected by default.";


	wxMessageDialog d(NULL, str, "Question", wxOK | wxCANCEL);
	int answer = d.ShowModal();

	if(answer == wxID_OK)
	{
		wxTextFile rigFile(RIG_FILE);
		rigFile.Open(RIG_FILE);
		rigFile.Clear();
		switch ( room )
		{
			case MOOG_164:
				rigFile.AddLine("MOOG_164");
				break;
			case MOOG_157:
				rigFile.AddLine("MOOG_157");
				break;
			default:
				break;
		}
		rigFile.Write();
		rigFile.Close();
		
		m_mainFrame->OnFrameClose(wxCloseEvent());
	}
	else if(answer == wxID_CANCEL)
	{	// reselect old room
		m_radioBox->SetSelection(RIG_ROOM);
	}
}