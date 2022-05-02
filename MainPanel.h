#pragma once

#include "GlobalDefs.h"
#include "MoogDotsCom.h"

class CMainFrame;

class CMainPanel : public wxPanel
{
private:
	CMainFrame *m_mainFrame;
	wxButton *m_EngageButton,
			 *m_StopButton,
			 *m_ParkButton,
			 *m_ResetButton;
			 
	wxStaticBox *m_generalBox;				// Surrounds the main control buttons.
	int m_width, m_height;					// Width, height of this CMainPanel.
	wxListBox *m_moogListBox;				// Displays all available parameter list items.
	wxStaticText *m_descriptionText;		// Shows the parameter description.
	wxTextCtrl *m_dataTextBox;				// Displays the data associated with a parameter list key.
	wxListBox *m_messageConsole;			// Displays info about what the program is doing.
	wxButton *m_goButton,					// Executes the parameter list.
			 *m_setButton,					// Sets the data for a parameter.
			 *m_goToZeroButton;				// Makes the motion base move to zero position.
	MoogDotsCom *m_moogCom;
#if RECORD_MOVIE
	bool m_recordingMovie;
#endif
	// Sizers
	wxBoxSizer *m_topSizer,
			   *m_upperSizer,
			   *m_upperRightSizer,
			   *m_parameterSizer,
			   *m_otherButtonsSizer;
	wxStaticBoxSizer *m_buttonSizer;

public:
	wxRadioBox *m_radioBox;

public:
	CMainPanel(/*wxWindow*/CMainFrame *parent, wxWindowID id, MoogDotsCom *com);

	// Engages the motion base.
	void OnEngageButtonClicked(wxCommandEvent &event);

	// Parks the motion base.
	void OnParkButtonClicked(wxCommandEvent &event);

	// Resets the MBC.
	void OnResetButtonClicked(wxCommandEvent &event);

	// Performs actions whenever a list item is selected.
	void OnItemSelected(wxCommandEvent &event);

	// Starts a trial based on the current parameter list.
	void OnGoButtonClicked(wxCommandEvent &event);

	// Sets the data for the selected parameter list key.
	void OnSetButtonClicked(wxCommandEvent &event);

	// Stops all motion base movement.
	void OnStopButtonClicked(wxCommandEvent &event);

	// Moves the motion base back to zero position.
	void OnGoToZeroButtonClicked(wxCommandEvent &event);

	// Rig radio box control
	void OnRigSelected(wxCommandEvent &event);

	virtual bool Enable(bool enable = TRUE);

private:
	// Creates all buttons.
	void initButtons();

	// Creates the Parameter List stuff.
	void initParameterListStuff();

	enum
	{
		ENGAGE_BUTTON,
		STOP_BUTTON,
		MOOG_LISTBOX,
		MOOG_GO_BUTTON,
		MOOG_SET_BUTTON,
		MOOG_ZERO_BUTTON,
		RIG_RADIO_BOX,
		PARK_BUTTON,
		RESET_BUTTON
	};

private:
	DECLARE_EVENT_TABLE()
};
