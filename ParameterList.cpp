#include "StdAfx.h"
#include "ParameterList.h"

// Critical function lock for the parameter list.
wxCriticalSection g_paramLock;

// Global parameter list
CParameterList g_pList;

CParameterList::CParameterList()
{
	LoadHash();
}

void CParameterList::LoadHash()
{
	ParameterValue x;
	int i;


	/***************** Six value parameters *******************/
	x.data.clear();
	x.variable = false;
	for (i = 0; i < 6; i++) {
		x.data.push_back(0.0);
	}
	// Offsets that are added onto the center of rotation.
	x.description = "Offsets added to the center of rotation (x,y,z)cm (Platform/GL).";
	m_pHash.insert(ParameterKeyPair("ROT_CENTER_OFFSETS", x));

	// Fixation Cross setting
    x.data.at(0) = 0.0; //On/Off
	x.data.at(1) = 1.0; //Screen/Edge [0,1]
	x.data.at(2) = 2.0; //line length (cm)
	x.data.at(3) = 3.0; //line width (pixel)
	x.data.at(4) = 0.0; //Shadow [On/Off]
	x.data.at(5) = 7.0; //Shadow line width (pixel)
	x.description = "FP cross [On/Off]; Screen/Edge[0,1]; length(cm); width(pixel); Shadow[On/Off]; Shadow width(pixel)";
	m_pHash.insert(ParameterKeyPair("FP_CROSS", x));

	/***************** Five value parameters *******************/
	x.data.clear();
	x.variable = false;
	for (i = 0; i < 5; i++) {
		x.data.push_back(0.0);
	}
	// Drawing vertical or horizontal or both line of target.
	x.description = "Measure tran amp: [Horizontal,Vertical] lines, ON/OFF=1.0/0.0; location:(x,y,z)";
	m_pHash.insert(ParameterKeyPair("CALIB_TRAN_ON", x));

	/***************** Four value parameters *******************/
	x.data.clear();
	for (i = 0; i < 4; i++) {
		x.data.push_back(0.0);
	}

	// Platform excursions for 2 interval movement.
	x.data.at(0) = 0.1; x.data.at(1) = 0.1; x.data.at(2) = 0.1; x.data.at(3) = 0.1;
	x.description = "2 interval excursions (m)";
	m_pHash.insert(ParameterKeyPair("2I_DIST", x));

	// Duration for each 2 interval movement.
	x.data.at(0) = 2.0; x.data.at(1) = 2.0; x.data.at(2) = 2.0; x.data.at(3) = 2.0;
	x.description = "2 interval duration (s)";
	m_pHash.insert(ParameterKeyPair("2I_TIME", x));

	// Sigma for each 2 interval movement.
	x.data.at(0) = 3.0; x.data.at(1) = 3.0; x.data.at(2) = 3.0; x.data.at(3) = 3.0;
	x.description = "2 interval sigma";
	m_pHash.insert(ParameterKeyPair("2I_SIGMA", x));

	// Elevations for the 2 interval movement.
	x.data.at(0) = 0.0; x.data.at(1) = 0.0; x.data.at(2) = 0.0; x.data.at(3) = 0.0;
	x.description = "2 interval elevation (deg)";
	m_pHash.insert(ParameterKeyPair("2I_ELEVATION", x));

	// Azimuths for the 2 interval movement.
	x.data.at(0) = 90.0; x.data.at(1) = 90.0; x.data.at(2) = 90.0; x.data.at(3) = 90.0;
	x.description = "2 interval azimuth (deg)";
	m_pHash.insert(ParameterKeyPair("2I_AZIMUTH", x));

	// Measure square size on screen.
	x.data.at(0) = 0.0; x.data.at(1) = 10.0; x.data.at(2) = 0.0; x.data.at(3) = 0.0;
	x.description = "Calib Square: [On/Off=1/0,size(cm),center(x,y)]";
	m_pHash.insert(ParameterKeyPair("CALIB_SQUARE", x));

	// Parameters for drawing hollow sphere dots.
	x.data.at(0) = 50.0;	// radius (cm)
	x.data.at(1) = 0.05;	// dots/cm^2
	x.data.at(2) = 3.0;		// Gaussian mean (degree)
	x.data.at(3) = 1.0;		// Gaussian sigma (degree)
	x.description = "Para for dots' sphere surface: radius(cm), density(dots/cm^2), Gauss mean and sigma for dot size.";
	m_pHash.insert(ParameterKeyPair("DOTS_SPHERE_PARA", x));

	// Step Velocity - change Sinusoid rotation movement to constant rotation speed
	x.description = "Step velocity: ";
    x.data.at(0) = 10.0;	// Angular speed (deg/s)
	x.data.at(1) = 300;		// Duration (s)
	x.data.at(2) = 0.0;		// Azimuth (degree)
	x.data.at(3) = 90.0;		// Elevatin (degree)
	x.description = "Step velocity para: Angular speed(deg/s), Duration(s), Azimuth(deg), Elevation(deg).";
    m_pHash.insert(ParameterKeyPair("STEP_VELOCITY_PARA", x));

	// random seed for generating dots
	x.data.at(0) = 0;	x.data.at(1) = 0;	
	x.data.at(2) = 0;	x.data.at(3) = 0;
	x.description = "Random seed for generating dots";
    m_pHash.insert(ParameterKeyPair("DOTS_BIN_CORR_SEED", x));

	// Object default point of origin.
	x.description = "Object Origin (x, y, z, w) (m/deg->w=0/1)";
	x.data.at(0) = 0.0; x.data.at(1) = 0.0; x.data.at(2) = 0.0; x.data.at(3) = 0.0;
	CreateMultiSetParameter(x,  "OBJ_M_ORIGIN", NUM_OF_DYN_OBJ);
	x.description = "Static Object Origin (x, y, z, w) (m/deg->w=0/1)";
	x.data.at(0) = 0.05; x.data.at(1) = 0.05; x.data.at(2) = 0.0; x.data.at(3) = 0.0;
	CreateMultiSetParameter(x,  "STATIC_OBJ_M_ORIGIN", NUM_OF_STATIC_OBJ);

	// Object Starfield dimensions in cm.
	x.description = "Object Starfield dimensions (W, H, D, cm/deg=0/1).";
	x.data[0] = 5.0; x.data[1] = 5.0; x.data[2] = 5.0; x.data.at(3) = 0.0;
	//m_pHash.insert(ParameterKeyPair("OBJ_VOLUME", x));
	CreateMultiSetParameter(x,  "OBJ_VOLUME", NUM_OF_DYN_OBJ);
	x.description = "Static Object Starfield dimensions (W, H, D, cm/deg=0/1).";
	CreateMultiSetParameter(x,  "STATIC_OBJ_VOLUME", NUM_OF_STATIC_OBJ);

	// Stimulus Mask
	x.data.at(0) = 0.0; x.data.at(1) = 0.0; x.data.at(2) = 0.0; x.data.at(3) = 0.0;
	//x.data.at(0) = 5.0; x.data.at(1) = 0.0; x.data.at(2) = 5.0; x.data.at(3) = 0.0;
	x.description = "Draw stim inside stencil mask (H, V, Radius, 0/1=cm/deg)";
	m_pHash.insert(ParameterKeyPair("STIMULUS_MASK", x));

	/***************** Three value parameters *******************/
	x.data.clear();
	for (i = 0; i < 3; i++) {
		x.data.push_back(0.0);
	}

	// Default point of origin.
	x.description = "Point of Origin (x, y, z) (m)";
	m_pHash.insert(ParameterKeyPair("M_ORIGIN", x));

	// Global coordinates for the center of the platform.
	x.data.at(0) = 0.0; x.data.at(1) = 0.0; x.data.at(2) = 0.0;
	x.description = "Platform center coordinates.";
	m_pHash.insert(ParameterKeyPair("PLATFORM_CENTER", x));

	// Global coordinates for the center of the head.
	x.data.at(0) = 0.0; x.data.at(1) = 0.0; x.data.at(2) = 0.0;
	x.description = "Center of head based around cube center (cm).";
	m_pHash.insert(ParameterKeyPair("HEAD_CENTER", x));

	// Coordinates for the rotation origin.
	x.data.at(0) = 0.0; x.data.at(1) = 0.0; x.data.at(2) = 0.0;
	x.description = "Rotation origin (yaw, pitch, roll) (deg).";
	m_pHash.insert(ParameterKeyPair("ROT_ORIGIN", x));

	// Starfield dimensions in cm.
	x.description = "Starfield dimensions in cm.";
	x.data[0] = 100.0; x.data[1] = 100.0; x.data[2] = 50.0;
	m_pHash.insert(ParameterKeyPair("STAR_VOLUME", x));

	// Object Starfield pattern (number of separation in each dim).
	x.description = "Object drawing pattern: No. of segments cuboid(nW nH nD) or ellipsoid (nR, nAzi, nEle).";
	x.data[0] = 2.0; x.data[1] = 4.0; x.data[2] = 4.0;
	//m_pHash.insert(ParameterKeyPair("OBJ_STAR_PATTERN", x));
	CreateMultiSetParameter(x,  "OBJ_STAR_PATTERN", NUM_OF_DYN_OBJ);
	x.description = "Static Object drawing pattern: No. of segments cuboid(nW nH nD) or ellipsoid (nR, nAzi, nEle).";
	CreateMultiSetParameter(x,  "STATIC_OBJ_STAR_PATTERN", NUM_OF_STATIC_OBJ);

	// X-coordinate for fixation point, target 1 & 2.
	x.description = "Target x-coordinates.";
	x.data[0] = 0.0; x.data[1] = 0.0; x.data[2] = 0.0;
	m_pHash.insert(ParameterKeyPair("TARG_XCTR", x));

	// Y-coordinate for fixation point, target 1 & 2.
	x.description = "Target y-coordinates.";
	x.data[0] = 0.0; x.data[1] = 0.0; x.data[2] = 0.0;
	m_pHash.insert(ParameterKeyPair("TARG_YCTR", x));

	// Z-coordinate for fixation point, target 1 & 2.
	x.description = "Target z-coordinates.";
	x.data[0] = 0.0; x.data[1] = 0.0; x.data[2] = 0.0;
	m_pHash.insert(ParameterKeyPair("TARG_ZCTR", x));

	// X-dim(deg) for fixation point, target 1 & 2.
	x.description = "X-dim(deg) for fixation point, target 1 & 2.";
	x.data[0] = 0.3; x.data[1] = 0.3; x.data[2] = 0.3;
	m_pHash.insert(ParameterKeyPair("TARG_XSIZ", x));

	// Y-dim(deg) for fixation point, target 1 & 2.
	x.description = "Y-dim(deg) for fixation point, target 1 & 2.";
	x.data[0] = 0.3; x.data[1] = 0.3; x.data[2] = 0.3;
	m_pHash.insert(ParameterKeyPair("TARG_YSIZ", x));

	// Shape(0=Ellipse or 1=Rect.) for fixation point, target 1 & 2.
	x.description = "Shape(0=Ellipse, 1=Rect, 2=Cross) for fixation point, target 1 & 2.";
	x.data[0] = 0.0; x.data[1] = 0.0; x.data[2] = 0.0;
	m_pHash.insert(ParameterKeyPair("TARG_SHAPE", x));

	// Red luminance(0->1) for fixation point, target 1 & 2.
	x.description = "Red luminance(0->1) for fixation point, target 1 & 2.";
	x.data[0] = 1.0; x.data[1] = 0.0; x.data[2] = 0.0;
	m_pHash.insert(ParameterKeyPair("TARG_RLUM", x));

	// Green luminance(0->1) for fixation point, target 1 & 2.
	x.description = "Green luminance(0->1) for fixation point, target 1 & 2.";
	x.data[0] = 1.0; x.data[1] = 0.0; x.data[2] = 0.0;
	m_pHash.insert(ParameterKeyPair("TARG_GLUM", x));

	// Blue luminance(0->1) for fixation point, target 1 & 2.
	x.description = "Blue luminance(0->1) for fixation point, target 1 & 2.";
	x.data[0] = 0.0; x.data[1] = 0.0; x.data[2] = 0.0;
	m_pHash.insert(ParameterKeyPair("TARG_BLUM", x));

	// Luminous mult for fixation point, target 1 & 2.
	x.description = "Luminous mult for fixation point, target 1 & 2";
	x.data[0] = 1.0; x.data[1] = 1.0; x.data[2] = 1.0;
	m_pHash.insert(ParameterKeyPair("TARG_LUM_MULT", x));

	// Color for the left star scene.
	x.description = "Color for the left-eye stars.";
	x.data[0] = 1.0; x.data[1] = 0.0; x.data[2] = 0.0;
	m_pHash.insert(ParameterKeyPair("STAR_LEYE_COLOR", x));

	// Color for the right star scene.
	x.description = "Color for the right-eye stars.";
	x.data[0] = 0.0; x.data[1] = 1.0; x.data[2] = 0.0;
	m_pHash.insert(ParameterKeyPair("STAR_REYE_COLOR", x));

	// Magnitude for the noise.
	x.description = "Noise magnitude (std deviations).";
	x.data[0] = 0.01; x.data[1] = 0.01; x.data[2] = 0.01;
	m_pHash.insert(ParameterKeyPair("NOISE_MAGNITUDE", x));

	// Location of the monocular eye measured from the center of the head.
	x.data.at(0) = 0.0; x.data.at(1) = 0.0; x.data.at(2) = 0.0;
	x.description = "Location of the monocular eye measured from the center of the head. (x, y, z)cm.";
	m_pHash.insert(ParameterKeyPair("EYE_OFFSETS", x));

	// Color for the left object.
	x.description = "Color for the left-eye object.";
	x.data[0] = 1.0; x.data[1] = 0.0; x.data[2] = 0.0;
	//m_pHash.insert(ParameterKeyPair("OBJ_LEYE_COLOR", x));
	CreateMultiSetParameter(x,  "OBJ_LEYE_COLOR", NUM_OF_DYN_OBJ);
	x.description = "Color for the left-eye static object.";
	CreateMultiSetParameter(x,  "STATIC_OBJ_LEYE_COLOR", NUM_OF_STATIC_OBJ);

	// Color for the right object.
	x.description = "Color for the right-eye object.";
	x.data[0] = 0.0; x.data[1] = 1.0; x.data[2] = 0.0;
	//m_pHash.insert(ParameterKeyPair("OBJ_REYE_COLOR", x));
	CreateMultiSetParameter(x,  "OBJ_REYE_COLOR", NUM_OF_DYN_OBJ);
	x.description = "Color for the right-eye static object.";
	CreateMultiSetParameter(x,  "STATIC_OBJ_REYE_COLOR", NUM_OF_STATIC_OBJ);

	// couout texture setting
	x.description = "Cutout texture: line width(pixel), line separation(cm), rotation angle(deg)";
	x.data[0] = 1.0; x.data[1] = 1.0; x.data[2] = 0.0;
	m_pHash.insert(ParameterKeyPair("CUTOUT_TEXTURE_PARAS", x));

	// Translation distance of object copies.
	x.description = "Translation distance(xyz) of object copies in cm";
	x.data[0] = 0.0; x.data[1] = 0.0; x.data[2] = 0.0;
	CreateMultiSetParameter(x, "OBJ_COPY_TRANS", NUM_OF_DYN_OBJ*MAX_DYN_OBJ_COPY_NUM);

	// Near and far clipping planes.
	x.description = "Clipping planes: Near and Far (unit in 0/1=cm/deg).";
	x.data[0] = 5.0;	// Near
	x.data[1] = 150.0;	// Far
	x.data[2] = 0.0;	// unit in 0/1=m/deg
	m_pHash.insert(ParameterKeyPair("CLIP_PLANES", x));

	/***************** Two value parameters *****************/
	x.data.clear();
	for (i = 0; i < 2; i++) {
		x.data.push_back(0.0);
	}

	// Delay between the 1st and 2nd 2I movement.
	x.description = "Delay between the 2I movement (s).";
	x.data.at(0) = 0.0; x.data.at(1) = 0.0;
	m_pHash.insert(ParameterKeyPair("2I_DELAY", x));

	// Screen dimensions.
	x.description = "Screen Width and Height (cm).";
	x.data[0] = 58.8;	// Width 
	x.data[1] = 58.9;	// Height
	m_pHash.insert(ParameterKeyPair("SCREEN_DIMS", x));

	// Near and far clipping planes.
	//x.description = "Near and Far clipping planes (cm).";
	//x.data[0] = 5.0;	// Near
	//x.data[1] = 150.0;	// Far
	//m_pHash.insert(ParameterKeyPair("CLIP_PLANES", x));

	// Triangle dimensions.
	x.description = "Triangle Base and Height (cm).";
#if DEBUG_DEFAULTS
	x.data[0] = 0.3; x.data[1] = 0.3;
#else
	x.data[0] = 0.15; x.data[1] = 0.15;
#endif
	m_pHash.insert(ParameterKeyPair("STAR_SIZE", x));

	// Object Triangle dimensions.
	x.description = "Object Triangle Base and Height (cm).";
#if DEBUG_DEFAULTS
	x.data[0] = 0.3; x.data[1] = 0.3;
#else
	x.data[0] = 0.15; x.data[1] = 0.15;
#endif
	//m_pHash.insert(ParameterKeyPair("OBJ_STAR_SIZE", x));
	CreateMultiSetParameter(x,  "OBJ_STAR_SIZE", NUM_OF_DYN_OBJ);
	x.description = "Static Object Triangle Base and Height (cm).";
	CreateMultiSetParameter(x,  "STATIC_OBJ_STAR_SIZE", NUM_OF_STATIC_OBJ);

	// Elevation
	x.description = "Elevation in degrees (Base/GL).";
	x.data[0] = -90.0; x.data[1] = -90.0;
	m_pHash.insert(ParameterKeyPair("M_ELEVATION", x));

	// Azimuth
	x.description = "Azimuth in degrees (Base/GL).";
	x.data[0] = 0.0; x.data[1] = 0.0;
	m_pHash.insert(ParameterKeyPair("M_AZIMUTH", x));

	// Noise Elevation
	x.description = "Noise Elevation in degrees (Base/GL).";
	x.data[0] = 0.0; x.data[1] = 0.0;
	m_pHash.insert(ParameterKeyPair("NOISE_ELEVATION", x));

	// Noise Azimuth
	x.description = "Noise Azimuth in degrees (Base/GL).";
	x.data[0] = 0.0; x.data[1] = 0.0;
	m_pHash.insert(ParameterKeyPair("NOISE_AZIMUTH", x));

	// Distance travelled my the motion base.
	x.description = "Movement Magnitude (m) (Base/GL).";
	x.data[0] = 0.10; x.data[1] = 0.10;
	m_pHash.insert(ParameterKeyPair("M_DIST", x));

	// Default movement duration.
	x.data[0] = 2.0; x.data[1] = 2.0;
	x.description = "Movement Duration x (s) (Base/GL).";
	m_pHash.insert(ParameterKeyPair("M_TIME", x));

	// onset delay ratio of movement duration.
	x.data[0] = 0.0; x.data[1] = 0.0;
	x.description = "Onset delay ratio(0-1) of movement duration (Base/GL).";
	m_pHash.insert(ParameterKeyPair("M_TIME_ONSET_DELAY", x));

	// offset delay ratio of movement duration.
	x.data[0] = 1.0; x.data[1] = 1.0;
	x.description = "Offset delay ratio(0-1) of movement duration (Base/GL).";
	m_pHash.insert(ParameterKeyPair("M_TIME_OFFSET_DELAY", x));

	// Number of sigmas in the Gaussian.
	x.data[0] = 3.0; x.data[1] = 3.0;
	x.description = "Number of sigmas in the Gaussian (Base/GL).";
	m_pHash.insert(ParameterKeyPair("M_SIGMA", x));

	/*
	// Number of cycles of sine wave in the Gabor.
	x.data[0] = 1.0; x.data[1] = 1.0;
	x.description = "Number of sine wave in the Gabor (Base/GL).";
	m_pHash.insert(ParameterKeyPair("M_CYCLE", x));
	*/

	// Elevation of the axis of rotation.
#if DEBUG_DEFAULTS
	x.data.at(0) = 0.0;
	x.data.at(1) = 0.0;
#else
	x.data.at(0) = -90.0;
	x.data.at(1) = -90.0;
#endif
	x.description = "Axis of rotation elevation.";
	m_pHash.insert(ParameterKeyPair("ROT_ELEVATION", x));

	// Azimuth of the axis of rotation.
#if DEBUG_DEFAULTS
	x.data.at(0) = 0.0;
	x.data.at(1) = 0.0;
#else
	x.data.at(0) = 0.0;
	x.data.at(1) = 0.0;
#endif
	x.description = "Axis of rotation azimuth.";
	m_pHash.insert(ParameterKeyPair("ROT_AZIMUTH", x));

	// Amplitude of rotation.
#if DEBUG_DEFAULTS
	x.data.at(0) = 5.0;
	x.data.at(1) = 5.0;
#else
	x.data.at(0) = 5.0;
	x.data.at(1) = 5.0;
#endif
	x.description = "Amplitude of rotation.";
	m_pHash.insert(ParameterKeyPair("ROT_AMPLITUDE", x));

	// Duration of rotation.
#if DEBUG_DEFAULTS
	x.data.at(0) = 2.0;
	x.data.at(1) = 2.0;
#else
	x.data.at(0) = 2.0;
	x.data.at(1) = 2.0;
#endif
	x.description = "Duration of rotation.";
	m_pHash.insert(ParameterKeyPair("ROT_DURATION", x));

	// onset delay ratio of movement duration.
	x.data[0] = 0.0; x.data[1] = 0.0;
	x.description = "Onset delay ratio(0-1) of rotation duration (Base/GL).";
	m_pHash.insert(ParameterKeyPair("ROT_DURATION_ONSET_DELAY", x));

	// offset delay ratio of movement duration.
	x.data[0] = 1.0; x.data[1] = 1.0;
	x.description = "Offset delay ratio(0-1) of rotation duration (Base/GL).";
	m_pHash.insert(ParameterKeyPair("ROT_DURATION_OFFSET_DELAY", x));


	// Number of sigmas in the Gaussian rotation.
#if DEBUG_DEFAULTS
	x.data.at(0) = 6.0;
	x.data.at(1) = 6.0;
#else
	x.data.at(0) = 3.0;
	x.data.at(1) = 3.0;
#endif
	x.description = "Number of sigmas in rot Gaussian.";
	m_pHash.insert(ParameterKeyPair("ROT_SIGMA", x));

	// Phase of rotation.
	x.data.at(0) = 0.0;
	x.data.at(1) = 0.0;
	x.description = "Phase of rotation (deg).";
	m_pHash.insert(ParameterKeyPair("ROT_PHASE", x));

	// Rotation start offset
	x.data.at(0) = 0.0;
	x.data.at(1) = 0.0;
	x.description = "Rotation start offset (deg).";
	m_pHash.insert(ParameterKeyPair("ROT_START_OFFSET", x));


	// Amplitude of the translational sinusoid (m).
	x.data.at(0) = 0.05; x.data.at(1) = 0.05;
	x.description = "Sinusoid translational amplitude (m).";
	m_pHash.insert(ParameterKeyPair("SIN_TRANS_AMPLITUDE", x));

	// Amplitude of the rotational sinusoid (deg).
	x.data.at(0) = 5.0; x.data.at(1) = 5.0; 
	x.description = "Sinusoid rotational amplitude (deg).";
	m_pHash.insert(ParameterKeyPair("SIN_ROT_AMPLITUDE", x));

	// Frequency of the sinusoid (Hz).
	x.data.at(0) = 0.5; x.data.at(1) = 0.5;
	x.description = "Sinusoid frequency (Hz)";
	m_pHash.insert(ParameterKeyPair("SIN_FREQUENCY", x));

	// Elevation of the sinusoid (deg).
	x.data.at(0) = 0.0; x.data.at(1) = 0.0; 
	x.description = "Sinusoid elevation (deg)";
	m_pHash.insert(ParameterKeyPair("SIN_ELEVATION", x));

	// Azimuth of the sinusoid (deg).
	x.data.at(0) = 0.0; x.data.at(1) = 0.0;
	x.description = "Sinusoid azimuth (deg)";
	m_pHash.insert(ParameterKeyPair("SIN_AZIMUTH", x));

	// Duration of the sinusoid (s)
	x.data.at(0) = 60.0*5.0; x.data.at(1) = 60*5.0;
	x.description = "Sinusoid duration (s)";
	m_pHash.insert(ParameterKeyPair("SIN_DURATION", x));

	// Normalization factors for the CED analog output.
	x.description = "Normalization factors [min, max].";
	x.data.at(0) = 0.0; x.data.at(1) = 1.0;
	m_pHash.insert(ParameterKeyPair("STIM_NORM_FACTORS", x));

	// Help us to measure the amplitude of sinusoid rotation motion
	x.description = "Measure rot amp: [Fixed, Move] lines, ON/OFF=1.0/0.0";
	x.data.at(0) = 0.0; x.data.at(1) = 0.0;
	m_pHash.insert(ParameterKeyPair("CALIB_ROT_ON", x));

	// Help us to measure the amplitude of sinusoid rotation motion
	x.description = "Measurement line setup: width(pixel) and length(cm)";
	x.data.at(0) = 2.0; x.data.at(1) = 10.0;
	m_pHash.insert(ParameterKeyPair("CALIB_LINE_SETUP", x));

	// Setup object rotation vector
	x.description = "Object rotation vector: Azi and Ele in degree.";
	x.data.at(0) = 0.0; x.data.at(1) = 90.0;
	//m_pHash.insert(ParameterKeyPair("OBJ_ROT_VECTOR", x));
	CreateMultiSetParameter(x,  "OBJ_ROT_VECTOR", NUM_OF_DYN_OBJ);
	x.description = "Static Object rotation vector: Azi and Ele in degree.";
	CreateMultiSetParameter(x,  "STATIC_OBJ_ROT_VECTOR", NUM_OF_STATIC_OBJ);

	// trapezoid middle time stamp
	x.description = "(Ratio of M_TIME) Start time for const trapezoid velocity movement.(Base/GL)";
	x.data.at(0) = 0.5; x.data.at(1) = 0.5;
	m_pHash.insert(ParameterKeyPair("TRAPEZOID_TIME1", x));

	x.description = "(Ratio of M_TIME) End time for const trapezoid velocity movement.(Base/GL)";
	x.data.at(0) = 0.5; x.data.at(1) = 0.5;
	m_pHash.insert(ParameterKeyPair("TRAPEZOID_TIME2", x));

	// trapezoid middle time stamp for rotation
	x.description = "(Ratio of M_TIME) Start time for const trapezoid velocity rotation.(Base/GL)";
	x.data.at(0) = 0.5; x.data.at(1) = 0.5;
	m_pHash.insert(ParameterKeyPair("TRAPEZOID_ROT_T1", x));

	x.description = "(Ratio of M_TIME) End time for const trapezoid velocity rotation.(Base/GL)";
	x.data.at(0) = 0.5; x.data.at(1) = 0.5;
	m_pHash.insert(ParameterKeyPair("TRAPEZOID_ROT_T2", x));

	// Patch Depth Range
	x.description = "Depth Range of dot patch (°)";
	x.data[0] = -2; 	x.data[1] = 2;
	m_pHash.insert(ParameterKeyPair("PATCH_DEPTH_RANGE", x));

	// Pseudo depth of object
	x.description = "Object: (Pseudo depth, unit in 0/1=m/deg)";
	x.data[0] = 0.0;	x.data[1] = 0.0;
	CreateMultiSetParameter(x,  "OBJ_PSEUDO_DEPTH", NUM_OF_DYN_OBJ);

	// Object Starfield density (stars/cm^3).
	x.description = "Object Starfield Density (stars/cm^3), unit in 0/1/2=cm/deg/stars total#";
	x.data[0] = 1.0;	x.data[1] = 0.0;
	//m_pHash.insert(ParameterKeyPair("OBJ_STAR_DENSITY", x));
	CreateMultiSetParameter(x,  "OBJ_STAR_DENSITY", NUM_OF_DYN_OBJ);
	x.description = "Static Object Starfield Density (stars/cm^3), unit in 0/1/2=cm/deg/stars total#";
	CreateMultiSetParameter(x,  "STATIC_OBJ_STAR_DENSITY", NUM_OF_STATIC_OBJ);

	/***************** One value parameters *****************/
	x.data.clear();
	x.data.push_back(0.0);

	// Flags a circle cutout to appear at the center of the screen.
#if DEBUG_DEFAULTS
	x.data.at(0) = 0.0;
#else
	x.data.at(0) = 0.0;
#endif
	x.description = "Flags circle cutout.";
	m_pHash.insert(ParameterKeyPair("USE_CUTOUT", x));

	x.description = "Use texture in the mask of circle cutout.";
	x.data.at(0) = 0.0;
	m_pHash.insert(ParameterKeyPair("USE_CUTOUT_TEXTURE", x));

	// Setup condition for output velocity curve to CED
	x.description = "Output velocity curve to CED. true=1.0,fales=0.0";
	x.data[0] = 0.0;
	m_pHash.insert(ParameterKeyPair("OUTPUT_VELOCITY", x));

	// Output to D/A board of the stumulus; sent to the Moog and the frame delayed opengl singal.
	x.description = "Signal output to D/A board. [Lateral,Heave,Surge,Yaw,Pitch,Roll]=[1,2,3,4,5,6]";
	x.data[0] = 0.0;
	m_pHash.insert(ParameterKeyPair("STIM_ANALOG_OUTPUT", x));

	// Setup condition for output amplitude curve to CED
	x.description = "Mult. of Signal output to D/A board";
	x.data[0] = 1.0;
	m_pHash.insert(ParameterKeyPair("STIM_ANALOG_MULT", x));

	// The radius of the cutout.
	x.data.at(0) = 5.0;
	x.description = "Cutout radius (cm)";
	m_pHash.insert(ParameterKeyPair("CUTOUT_RADIUS", x));

	// The radius of the cutout.
	x.data.at(0) = 0.0;
	x.description = "Cutout starfield mode (0=none, 1=planar, 2=cylinder, 3=cylinder+random depth";
	m_pHash.insert(ParameterKeyPair("CUTOUT_STARFIELD_MODE", x));

	// How to locate dots. branched from STARS_TYPE. 
	// initially, DRAW_MODE was STARS_TYPE in TEMPO. so it contained both shape and location.
	// here I make parameter for location, so it would be good to move sphere mode 
	// to STARTS_LOCATION later.
	x.data.at(0) = 0.0;
	x.description = "Stars Location (1=random, 2=cylinder depth";
	m_pHash.insert(ParameterKeyPair("STAR_LOCATION", x));

	// Determines if sinusoidal motion is continuous.
	x.data.at(0) = 0.0; 
	x.description = "Sin continuous.  0 = no, 1 = yes";
	m_pHash.insert(ParameterKeyPair("SIN_CONTINUOUS", x));

	// Sets whether rotation is translational or rotational.
	x.data.at(0) = 0.0; 
	x.description = "Sin Mode.  0 = trans, 1 = rot";
	m_pHash.insert(ParameterKeyPair("SIN_MODE", x));

#if DEBUG_DEFAULTS
	x.data.at(0) = 1.0;
#else
	x.data.at(0) = 0.0;
#endif
	x.description = "Toggle fixation point rotation.";
	m_pHash.insert(ParameterKeyPair("FP_ROTATE", x));
	//0: don't rotate FP
	//1: Rotate the fixation point.
	//2: pursue case: eye mask moving with FP
	//3: Simulated pursue case: FP and cutoff mask will be at center and eye mask will not move.

	// Delay after the sync.
	x.data.at(0) = 0.0;
	x.description = "Delay (ms) after the sync.";
	m_pHash.insert(ParameterKeyPair("SYNC_DELAY", x));

	// Mode for the tilt/translation.
	x.data.at(0) = 0.0;
	x.description = "Mode for t/t.";
	m_pHash.insert(ParameterKeyPair("TT_MODE", x));

	// Sets how many dimensions of noise we want to use.
	x.description = "Number of noise dimensions (1,2, or 3).";
	x.data[0] = 1.0;
	m_pHash.insert(ParameterKeyPair("NOISE_DIMS", x));

	// Determines whether or not we multiply the noise by a high
	// powered Gaussian to make the ends zero.
	x.description = "Fix noise (1=yes,0=no).";
	x.data[0] = 1.0;
	m_pHash.insert(ParameterKeyPair("FIX_NOISE", x));

	// Determines if we multiply the filter by a high powered
	// Gaussian.
	x.description = "Fix filter (1=yes,0=no).";
	x.data[0] = 1.0;
	m_pHash.insert(ParameterKeyPair("FIX_FILTER", x));

	// Filter frequency.
	x.description = "Filter frequency (.1-10Hz).";
	x.data[0] = 5.0;
	m_pHash.insert(ParameterKeyPair("CUTOFF_FREQ", x));

	// Gaussian normal distribution seed.
	x.description = "Gaussian normal distribution seed > 0";
	x.data[0] = 1978.0;
	m_pHash.insert(ParameterKeyPair("GAUSSIAN_SEED", x));

	// Star lifetime.
	x.description = "Star lifetime (#frames).";
	x.data[0] = 5.0;
	m_pHash.insert(ParameterKeyPair("STAR_LIFETIME", x));

	// Star motion coherence factor.  0 means all stars change.
	x.description = "Star motion coherence (% out of 100).";
	x.data[0] = 15.0;
	m_pHash.insert(ParameterKeyPair("STAR_MOTION_COHERENCE", x));
	
	// Turns star lifetime on and off.
	x.description = "Star lifetime on/off.";
	x.data[0] = 0.0;
	m_pHash.insert(ParameterKeyPair("STAR_LIFETIME_ON", x));

	// Star luminance multiplier.
	x.description = "Star luminance multiplier.";
	x.data[0] = 1.0;
	m_pHash.insert(ParameterKeyPair("STAR_LUM_MULT", x));

	// Target 1 on/off.
	x.description = "Target 1 on/off.";
	x.data[0] = 0.0;
	m_pHash.insert(ParameterKeyPair("TARG1_ON", x));

	// Target 2 on/off.
	x.description = "Target 2 on/off.";
	x.data[0] = 0.0;
	m_pHash.insert(ParameterKeyPair("TARG2_ON", x));
	
	// Fixation point on/off.
	x.description = "Fixation point on/off.";
#if DEBUG_DEFAULTS
	x.data[0] = 1.0;
#else
	x.data[0] = 0.0; 
#endif
	m_pHash.insert(ParameterKeyPair("FP_ON", x));

	// Decides if noise should be added.
	x.description = "Enables noise. (0=off, 1=on)";
	x.data[0] = 0.0;
	m_pHash.insert(ParameterKeyPair("USE_NOISE", x));

	// Turns movement on and off.
	x.description = "Enables motion base movement. (0.0=off, 1.0==on, -1.0=cal)";
	x.data[0] = 0.0;
	m_pHash.insert(ParameterKeyPair("DO_MOVEMENT", x));

	x.description = "Makes the motion base move to zero position.";
	x.data[0] = 0.0;
	m_pHash.insert(ParameterKeyPair("GO_TO_ZERO", x));


	// The amount of offset given to a library trajectory.
	// This value will reset in MoogDots::InitRig().
	x.description = "Offset used to shift predicted trajectory (ms).";
	x.data.at(0) = 500.0;
	m_pHash.insert(ParameterKeyPair("PRED_OFFSET", x));

	// The amount of offset given to frame delay of different projectors,
	// so that we can generate a same signal as accelerometer.
	// This value will reset in MoogDots::InitRig().
	x.description = "Offset used to adjust frame delay of different projectors (ms).";
	x.data.at(0) = 16.0;
	m_pHash.insert(ParameterKeyPair("FRAME_DELAY", x));

	/*
	// Indicates if the target should be on.
	x.description = "Indicates if the target should be on.";
	x.data[0] = 0.0;
	m_pHash.insert(ParameterKeyPair("TARG_CROSS", x));
	*/

	// Size of the center target.
	x.description = "Size of the center target.";
	x.data[0] = 7.0;
	m_pHash.insert(ParameterKeyPair("TARGET_SIZE", x));

	// Indicates if the background shoud be on.
	x.description = "Indicates if the background should be on.";
#if DEBUG_DEFAULTS
	x.data[0] = 1.0;
#else
	x.data[0] = 0.0; 
#endif
	m_pHash.insert(ParameterKeyPair("BACKGROUND_ON", x));

	// Center of dots' sphere is at monkey eye, but center of triangles' or dots' cube at screen.
	x.description = "Draw volume: 0)dots' soild sphere; 1)triangles' cube; 2)dots' cube; 3) dots' hollow sphere";
	x.data[0] = 1.0; 
	m_pHash.insert(ParameterKeyPair("DRAW_MODE", x));

	// Draw dots size.
	x.description = "Draw dots size.";
	x.data[0] = 4.0;
	m_pHash.insert(ParameterKeyPair("DOTS_SIZE", x));


	// Motion type.
	x.description = "Motion Type: 0=Gaussian, 1=Rotation, 2=Sinusoid, 3=2I, 4=TT, 5=Gabor, 6=Step Velocity, 7=Motion Parallax, 8=Trapezoid Velocity, 9=Tran plus Rot, 10=Trapezoid Tran plus Rot";
#if DEBUG_DEFAULTS
	x.data[0] = 10.0;
#else
	x.data[0] = 0.0; 
#endif
	m_pHash.insert(ParameterKeyPair("MOTION_TYPE", x));

	// Interocular distance.
	x.description = "Interocular distance (cm).";
#if DEBUG_DEFAULTS
	x.data.at(0) = 0.7;
#else
	x.data[0] = 6.5;
#endif
	m_pHash.insert(ParameterKeyPair("IO_DIST", x));

	// Starfield density (stars/cm^3).
	x.description = "Starfield Density (stars/cm^3).";
	x.data[0] = 0.01;
	m_pHash.insert(ParameterKeyPair("STAR_DENSITY", x));

	// The amount of offset given to analog output.
    x.description = "Offset used to shift predicted stimulus (ms)";
    x.data.at(0) = 465.00;
    m_pHash.insert(ParameterKeyPair("PRED_OFFSET_STIMULUS", x));

	// Help us to measure the amplitude of sinusoid rotation motion
	x.description = "Measure rot amp: 0=Pitch, 1=Yaw, 2=Roll";
	x.data.at(0) = 1.0;
	m_pHash.insert(ParameterKeyPair("CALIB_ROT_MOTION", x));

	// OpengGL refresh is synchronized to monitor freq/refresh.
	// as opposed to Moog communication freq.
    x.description = "Enable OpengGL refresh sync to monitor freq.";
    x.data.at(0) = 0.0;
    m_pHash.insert(ParameterKeyPair("VISUAL_SYNC", x));

	// Rendering type (0: general Render(), 1: RenderMotionParallax())
    x.description = "Rendering type";
    x.data.at(0) = 0.0;
    m_pHash.insert(ParameterKeyPair("RENDER_TYPE", x));

	/***** Object parameters *******/
	// Object enable
	x.description = "Enable object on/off (1/0).";
	x.data.at(0) = 0.0;
	//m_pHash.insert(ParameterKeyPair("OBJ_ENABLE", x));
	CreateMultiSetParameter(x,  "OBJ_ENABLE", NUM_OF_DYN_OBJ);
	x.description = "Dyn Obj targets: 1/0=On/Off";
	CreateMultiSetParameter(x,  "OBJ_TARG_ENABLE", NUM_OF_DYN_OBJ);
	x.description = "Enable static object on/off (1/0).";

	x.description = "Enable static object on/off (1/0).";
	CreateMultiSetParameter(x,  "STATIC_OBJ_ENABLE", NUM_OF_STATIC_OBJ);
	x.description = "Enable static target on/off (1/0).";
	CreateMultiSetParameter(x,  "STATIC_OBJ_TARG_ENABLE", NUM_OF_STATIC_OBJ);

	// Object motion type
	x.description = "Object motion type (0=Gaussian,1=Sin,2=Pseudo Depth,3=Retinal Const Speed,4=Abs. Const Speed,5=Trapezoid Velocity).";
	x.data.at(0) = 0.0;
	//m_pHash.insert(ParameterKeyPair("OBJ_MOTION_TYPE", x));
	CreateMultiSetParameter(x,  "OBJ_MOTION_TYPE", NUM_OF_DYN_OBJ);

	// Object distance travelled.
	x.data.resize(2);
	x.description = "Object Movement Magnitude (m), m/deg=0/1.";
	x.data.at(0) = 0.10; x.data.at(1) = 0.0;
	//m_pHash.insert(ParameterKeyPair("OBJ_M_DIST", x));
	CreateMultiSetParameter(x,  "OBJ_M_DIST", NUM_OF_DYN_OBJ);
	x.data.resize(1);

	// Object default movement duration.
	x.description = "Object Movement Duration x (s) (GL).";
	x.data.at(0) = 2.0;
	//m_pHash.insert(ParameterKeyPair("OBJ_M_TIME", x));
	CreateMultiSetParameter(x,  "OBJ_M_TIME", NUM_OF_DYN_OBJ);

	// Object - number of sigmas in the Gaussian.
	x.description = "Object - sigmas in the Gaussian (GL).";
	x.data.at(0) = 3.0;
	//m_pHash.insert(ParameterKeyPair("OBJ_M_SIGMA", x));
	CreateMultiSetParameter(x,  "OBJ_M_SIGMA", NUM_OF_DYN_OBJ);

	// Object moving direction
	x.description = "Azimuth angle (deg) - Object moving direction";
	x.data.at(0) = 0.0;
	//m_pHash.insert(ParameterKeyPair("OBJ_M_AZI", x));
	CreateMultiSetParameter(x,  "OBJ_M_AZI", NUM_OF_DYN_OBJ);

	x.description = "Elevation angle (deg) - Object moving direction";
	x.data.at(0) = 0.0;
	//m_pHash.insert(ParameterKeyPair("OBJ_M_ELE", x));
	CreateMultiSetParameter(x,  "OBJ_M_ELE", NUM_OF_DYN_OBJ);

	// Object lifetime.
	x.description = "Object lifetime (#frames).";
	x.data[0] = 5.0;
	//m_pHash.insert(ParameterKeyPair("OBJ_LIFETIME", x));
	CreateMultiSetParameter(x,  "OBJ_LIFETIME", NUM_OF_DYN_OBJ);


	// Object coherence factor.  0 means all stars in object change.
	x.description = "Object motion coherence (% out of 100).";
	x.data[0] = 70.0;
	//m_pHash.insert(ParameterKeyPair("OBJ_MOTION_COHERENCE", x));
	CreateMultiSetParameter(x,  "OBJ_MOTION_COHERENCE", NUM_OF_DYN_OBJ);
	
	// Turns Object lifetime on and off.
	x.description = "Object lifetime on/off (1/0).";
	x.data[0] = 0.0;
	//m_pHash.insert(ParameterKeyPair("OBJ_LIFETIME_ON", x));
	CreateMultiSetParameter(x,  "OBJ_LIFETIME_ON", NUM_OF_DYN_OBJ);

	// Object Star luminance multiplier.
	x.description = "Object star luminance multiplier.";
	x.data[0] = 1.0;
	//m_pHash.insert(ParameterKeyPair("OBJ_LUM_MULT", x));
	CreateMultiSetParameter(x,  "OBJ_LUM_MULT", NUM_OF_DYN_OBJ);
	x.description = "Static Object star luminance multiplier.";
	CreateMultiSetParameter(x,  "STATIC_OBJ_LUM_MULT", NUM_OF_STATIC_OBJ);

	// Object star type
	x.description = "Object star type (0=point, 1=triangles).";
	x.data[0] = 1.0;
	//m_pHash.insert(ParameterKeyPair("OBJ_STAR_TYPE", x));
	CreateMultiSetParameter(x,  "OBJ_STAR_TYPE", NUM_OF_DYN_OBJ);
	x.description = "Static Object star type (0=point, 1=triangles).";
	CreateMultiSetParameter(x,  "STATIC_OBJ_STAR_TYPE", NUM_OF_STATIC_OBJ);

	//// Object Starfield density (stars/cm^3).
	//x.description = "Object Starfield Density (stars/cm^3).";
	//x.data[0] = 1.0;
	////m_pHash.insert(ParameterKeyPair("OBJ_STAR_DENSITY", x));
	//CreateMultiSetParameter(x,  "OBJ_STAR_DENSITY", NUM_OF_DYN_OBJ);
	//x.description = "Static Object Starfield Density (stars/cm^3).";
	//CreateMultiSetParameter(x,  "STATIC_OBJ_STAR_DENSITY", NUM_OF_STATIC_OBJ);

	// Object dots size.
	x.description = "Object dots size in pixel";
	x.data[0] = 4.0;
	//m_pHash.insert(ParameterKeyPair("OBJ_DOTS_SIZE", x));
	CreateMultiSetParameter(x,  "OBJ_DOTS_SIZE", NUM_OF_DYN_OBJ);
	x.description = "Static Object dots size in pixel";
	CreateMultiSetParameter(x,  "STATIC_OBJ_DOTS_SIZE", NUM_OF_STATIC_OBJ);

	// Object shape
	x.description = "Object shape (0=ellipsoid, 1=cuboid).";
	x.data[0] = 0.0;
	//m_pHash.insert(ParameterKeyPair("OBJ_SHAPE", x));
	CreateMultiSetParameter(x,  "OBJ_SHAPE", NUM_OF_DYN_OBJ);
	x.description = "Static Object shape (0=ellipsoid, 1=cuboid).";
	CreateMultiSetParameter(x,  "STATIC_OBJ_SHAPE", NUM_OF_STATIC_OBJ);

	// Object star distribution
	x.description = "Object star distribution (0=random, 1=pattern).";
	x.data[0] = 0.0;
	//m_pHash.insert(ParameterKeyPair("OBJ_STAR_DISTRIBUTE", x));
	CreateMultiSetParameter(x,  "OBJ_STAR_DISTRIBUTE", NUM_OF_DYN_OBJ);
	x.description = "Static Object star distribution (0=random, 1=pattern).";
	CreateMultiSetParameter(x,  "STATIC_OBJ_STAR_DISTRIBUTE", NUM_OF_STATIC_OBJ);

	// Object body type
	x.description = "Object body type (0=soild, 1=hollow, 2=texture mapping).";
	x.data[0] = 0.0;
	//m_pHash.insert(ParameterKeyPair("OBJ_BODY_TYPE", x));
	CreateMultiSetParameter(x,  "OBJ_BODY_TYPE", NUM_OF_DYN_OBJ);
	x.description = "Static Object body type (0=soild, 1=hollow, 2=texture mapping).";
	CreateMultiSetParameter(x,  "STATIC_OBJ_BODY_TYPE", NUM_OF_STATIC_OBJ);

	// Object rotation speed
	x.description = "Object rotation speed (deg/s).";
	x.data[0] = 0.0;
	//m_pHash.insert(ParameterKeyPair("OBJ_ROT_SPEED", x));
	CreateMultiSetParameter(x,  "OBJ_ROT_SPEED", NUM_OF_DYN_OBJ);

	// Object initial rotation angle 
	x.description = "Object initial rotation angle in deg.";
	x.data[0] = 0.0;
	//m_pHash.insert(ParameterKeyPair("OBJ_INIT_ROT", x));
	CreateMultiSetParameter(x,  "OBJ_INIT_ROT", NUM_OF_DYN_OBJ);
	x.description = "Static Object initial rotation angle in deg.";
	CreateMultiSetParameter(x,  "STATIC_OBJ_INIT_ROT", NUM_OF_STATIC_OBJ);

	// Object luminance flag
	x.description = "Object luminance flag for changing lum (1/0=On/Off)";
	x.data[0] = 0.0;
	CreateMultiSetParameter(x, "OBJ_DYN_LUM_FLAG", NUM_OF_DYN_OBJ);

	// Object - number of sigmas in the Gaussian.
	x.description = "Object dyn lum - sigmas in the Gaussian (GL).";
	x.data.at(0) = 9.0;
	CreateMultiSetParameter(x, "OBJ_DYN_LUM_SIGMA", NUM_OF_DYN_OBJ);

	/*** Following parameters is added for object sin(Motion Parallax) movement ***/
	// Frequency
	x.description = "Object: Frequency of sinusoidal excursion (Hz)";
	x.data[0] = 0.5;
	CreateMultiSetParameter(x,  "OBJ_M_FREQUENCY", NUM_OF_DYN_OBJ);

	//// Preferred direction (We use obj elevation angle to replace this parameter.) - Johnny 7/29/2010
	//x.description = "Object: Preferred direction (°)";
	//x.data[0] = 0.0;
	//CreateMultiSetParameter(x,  "OBJ_PREF_DIRECTION", NUM_OF_DYN_OBJ);

	// Exponent
	x.description = "Object: Exponent of gaussian window";
	x.data[0] = 22.0;
	CreateMultiSetParameter(x,  "OBJ_M_EXPONENT", NUM_OF_DYN_OBJ);

	// Phase
	x.description = "Object: Starting movement phase (0° or 180° only, please)";
	x.data[0] = 0.0;
	CreateMultiSetParameter(x,  "OBJ_M_PHASE", NUM_OF_DYN_OBJ);
	/*** Above parameters is added for object sin(Motion Parallax) movement ***/

	// Pseudo depth of object
	//x.description = "Object: Pseudo depth(m)";
	//x.data[0] = 0.0;
	//CreateMultiSetParameter(x,  "OBJ_PSEUDO_DEPTH", NUM_OF_DYN_OBJ);

	// This object center cross checks where is center location of object. It works for all dynamic and static objects.
	x.description = "Draw center cross of all objects.";
	x.data[0] = 0.0;
	m_pHash.insert(ParameterKeyPair("OBJ_CENTER_CROSS", x));

	// Number of copies for object.
	x.description = "Number of copies for object.";
	x.data[0] = 0.0;
	CreateMultiSetParameter(x, "OBJ_NUM_OF_COPY", NUM_OF_DYN_OBJ);

	// Direction in depth gives an angle of original object moving direction.
	x.description = "Direction in depth (deg).";
	x.data[0] = 0.0;
	CreateMultiSetParameter(x, "OBJ_DIR_IN_DEPTH", NUM_OF_DYN_OBJ);
	
	/***** End of Object parameters *******/

	// Let Moog control timing.
	x.description = "Let Moog control timing";
	x.data[0] = 1.0;
	m_pHash.insert(ParameterKeyPair("MOOG_CTRL_TIME", x));

	// Enable mask when FP rotate
	x.description = "Enable mask when FP rotate, on/off (1/0)";
	x.data[0] = 0.0;
	m_pHash.insert(ParameterKeyPair("FP_ROTATE_MASK", x));


	/******** Motion Parallax parameters **********/
	// Two value parameters
	x.data.clear(); x.data.resize(2);

	// Coordinates for fixation point.
	x.description = "Fixation point location (x,y in °).";
	x.data[0] = 0.0; x.data[1] = 0.0;
	m_pHash.insert(ParameterKeyPair("FP_LOCATION", x));

	// Exponent
	x.description = "Exponent of gaussian window";
	x.data[0] = 22.0;
	x.data[1] = 22.0;
	m_pHash.insert(ParameterKeyPair("MOVE_EXPONENT", x));

	// Sigma
	x.description = "Sigma of gaussian window";
	x.data[0] = 55;
	x.data[1] = 55;
	m_pHash.insert(ParameterKeyPair("MOVE_SIGMA", x));

	// Phase
	x.description = "Starting movement phase (0° or 180° only, please)";
	x.data[0] = 0.0;
	x.data[1] = 0.0;
	m_pHash.insert(ParameterKeyPair("MOVE_PHASE", x));

	// Magnitude
	x.description = "Magnitude of sinsoidal excursion (m)";
	x.data[0] = 0.02;
	x.data[1] = 0.02;
	m_pHash.insert(ParameterKeyPair("MOVE_MAGNITUDE", x));

	// Frequency
	x.description = "Frequency of sinusoidal excursion (Hz)";
	x.data[0] = 0.5;
	x.data[1] = 0.5;
	m_pHash.insert(ParameterKeyPair("MOVE_FREQUENCY", x));

	// Preferred direction
	x.description = "Preferred direction (°)";
	x.data[0] = 0.0;
	x.data[1] = 0.0;
	m_pHash.insert(ParameterKeyPair("PREF_DIRECTION", x));

	// BG Dimensions
	x.description = "Background dimensions (XxY in °)";
	x.data[0] = 130.0; x.data[1] = 130.0;
	m_pHash.insert(ParameterKeyPair("BACKGROUND_SIZE", x));

	// Dot Patch Location (x,y)
	x.description = "Location of dot patch center (x,y in °)";
	x.data[0] = 0.0; x.data[1] = 0.0;
	m_pHash.insert(ParameterKeyPair("PATCH_LOCATION", x));

	// Viewport dimensions in pixels (XxY)
	x.description = "Viewport dimensions in pixels (XxY).";
	x.data[0] = 782.0;	// Width
	x.data[1] = 897.0;	// Height
	m_pHash.insert(ParameterKeyPair("VIEWPORT", x));

#if RECORD_MOVIE
	// Movie dimension in pixels (W, H)
	x.description = "Movie dimension in pixels (W, H). Only can modify in the beginning.";
	x.data[0] = GetSystemMetrics(SM_CXSCREEN);	// Width
	x.data[1] = GetSystemMetrics(SM_CYSCREEN);	// Height
	m_pHash.insert(ParameterKeyPair("MOVIE_DIM", x));
#endif

	/*** One value parameters ***/
	x.data.clear(); x.data.resize(1);

	// Disparity
	x.description = "Use binocular disparities (0,1,2,3)";
	x.data[0] = 1.0;
	m_pHash.insert(ParameterKeyPair("BINOC_DISP", x));

	// Patch drift
	x.description = "Drifting patch dots (deg/s)";
	x.data[0] = 2.0;
	m_pHash.insert(ParameterKeyPair("PATCH_DRIFT", x));

	// Patch coherence
	x.description = "gluLookAt follows fixation point (1=yes 0=no)";
	x.data[0] = 0.0;
	m_pHash.insert(ParameterKeyPair("GLU_LOOK_AT", x));

	// Patch coherence
	x.description = "Patch coherence (1=coherent 0=flicker)";
	x.data[0] = 1.0;
	m_pHash.insert(ParameterKeyPair("PATCH_COHERENCE", x));

	// Background coherence
	x.description = "Background coherence (1=coherent 0=flicker)";
	x.data[0] = 1.0;
	m_pHash.insert(ParameterKeyPair("BACKGROUND_COHERENCE", x));

	// Background stars
	x.description = "Density of background stars (dots/°^2)";
	x.data[0] = 0.000;
	m_pHash.insert(ParameterKeyPair("BACKGROUND_DENS", x));

	// BG Depth
	x.description = "Depth of background (°)";
	x.data[0] = 0.0;
	m_pHash.insert(ParameterKeyPair("BACKGROUND_DEPTH", x));

	// Alignment grid
	x.description = "Alignment grid on/off (°)";
	x.data[0] = 0.0;
	m_pHash.insert(ParameterKeyPair("ALIGN_ON", x));

	// Dot Patch stars
	x.description = "Density of dots in the patch (dots/°^2)";
	x.data[0] = 1.5;
	m_pHash.insert(ParameterKeyPair("PATCH_DENS", x));

	// Dot Patch diameter
	x.description = "Diameter of the dot patch (°)";
	x.data[0] = 40.0;
	m_pHash.insert(ParameterKeyPair("PATCH_DIAMETER", x));

	// Patch Depth
	x.description = "Depth of dot patch (°)";
	x.data[0] = 0.0;
	m_pHash.insert(ParameterKeyPair("PATCH_DEPTH", x));

	// Patch Binocular Correlation
	x.description = "Binocular correlation dot patch (%)";
	x.data[0] = 100;
	m_pHash.insert(ParameterKeyPair("PATCH_BIN_CORR", x));

	// Fixation point size.
	x.description = "Fixation point size in ° (0=off).";
	x.data[0] = 2.0;
	m_pHash.insert(ParameterKeyPair("FP_SIZE", x));

	// Fixation point flag.
	x.description = "Fixation point on/off.";
	x.data[0] = 0.0;

	// Duration
	x.description = "Duration of sinusoidal excursion (s)";
	x.data[0] = 2.0;  // Not sent by TEMPO!  Do not change! //Now, Tempo send this command and program uses it for both Moog and OpenGL . - Johnny 10/4/2010
	m_pHash.insert(ParameterKeyPair("MOVE_DURATION", x));

	// Distance from the monkey to the screen.
	// We don't use it anymore and use camera2screenDist. - Johnny 12/15/08 
	//f.camera2screenDist = CENTER2SCREEN - eyeOffsets.at(1) - headCenter.at(1);	// Distance from monkey to screen.
	//x.description = "Distance from the monkey to the screen (cm).";
	//x.data[0] = 31.9;
	//m_pHash.insert(ParameterKeyPair("VIEW_DIST", x));

	// Used to compensate for poor pursuit gain.
	x.description = "Monkey's pursuit gain.";
	x.data[0] = 1.0;
	m_pHash.insert(ParameterKeyPair("PURSUIT_GAIN", x));

	// Dot dimensions.
	x.description = "Dot size.";
	x.data[0] = 3.3;
	m_pHash.insert(ParameterKeyPair("DOT_SIZE", x));

	// Move to pixel
	x.description = "movetopixel conversion";
	x.data[0] = 8.5;
	m_pHash.insert(ParameterKeyPair("MOVETOPIXEL", x));

	// Eye to display patch (1: left, 2: right, 3: both)
	x.description = "Eye to display patch for Motion Palallax";
	x.data[0] = 3;
	m_pHash.insert(ParameterKeyPair("PATCH_DISP_EYE", x));

	// Eye to display fixation point (1: left, 2: right, 3: both)
	x.description = "Eye to display fixation point for Motion Parallax";
	x.data[0] = 3;
	m_pHash.insert(ParameterKeyPair("FIX_DISP_EYE", x));

	// different between binocular disparity and motion parallax depth (for InCong condition)
	x.description = "different between binocular disparity and motion parallax depth";
	x.data[0] = 0;
	m_pHash.insert(ParameterKeyPair("STEREO_MP_DIFF", x));

	/******** End of Motion Parallax parameters **********/

	//Send signal to Tempo for moving Fix Win; 1=rot FP; 2=MotionParallax
	x.description = "Send signal to Tempo for moving Fix Win; 1=rot FP; 2=MotionParallax";
	x.data[0] = 1;
	m_pHash.insert(ParameterKeyPair("FIX_WIN_MOVE_TYPE", x));

	//Control visual stimulus On/Off for each frame.
	x.description = "Control visual stimulus On/Off for each frame.";
	x.data[0] = 0;
	m_pHash.insert(ParameterKeyPair("CTRL_STIM_ON", x));

	//In translation motion, sometime we need FP stationary.
	x.description = "FP trans: 0=trans w camera, 1=stationary";
	x.data[0] = 0;
	m_pHash.insert(ParameterKeyPair("FP_TRANSLATION", x));

	//OpenGL scene rotation center flag.
	x.description = "OpenGL scene rotation center flag (0=head center, 1=cyclopean eye)";
	x.data[0] = 0;
	m_pHash.insert(ParameterKeyPair("GL_ROT_CENTER_FLAG", x));

	/*** Add the world reference object parameters ***/
	x.data.clear(); x.data.resize(1);
	
	//world reference object drawing ratio of each line
	x.description = "world reference drawing ratio";
	x.data[0] = 0.0;
	m_pHash.insert(ParameterKeyPair("W_REF_RATIO", x));

	//world reference line width in pixel
	x.description = "world reference line width in pixel";
	x.data[0] = 3.0;
	m_pHash.insert(ParameterKeyPair("W_REF_LINE_WIDTH", x));

	x.data.clear(); x.data.resize(3);

	//world reference left eye color
	x.description = "world reference left eye color";
	x.data[0] = 1.0; x.data[1] = 0.0; x.data[2] = 0.0;
	m_pHash.insert(ParameterKeyPair("W_REF_LEYE_COLOR", x));

	//world reference right eye color
	x.description = "world reference right eye color";
	x.data[0] = 0.0; x.data[1] = 1.0; x.data[2] = 0.0;
	m_pHash.insert(ParameterKeyPair("W_REF_REYE_COLOR", x));

	//world reference volume (x,y,z) in cm
	x.description = "world reference volume (x,y,z) in cm";
	x.data[0] = 10.0; x.data[1] = 10.0; x.data[2] = 10.0;
	m_pHash.insert(ParameterKeyPair("W_REF_VOLUME", x));

	//world reference origin (x,y,z) in cm
	x.description = "world reference origin (x,y,z) in cm";
	x.data[0] = 0.0; x.data[1] = 0.0; x.data[2] = 0.0;
	m_pHash.insert(ParameterKeyPair("W_REF_ORIGIN", x));
	
	/*** End of the world reference object parameters ***/

}

void CParameterList::CreateMultiSetParameter(ParameterValue x,  string paraName, int numOfSet)
{
	int numOfPara = x.data.size();
	for(int i=1; i<numOfSet; i++)
		for(int p=0; p<numOfPara; p++)
			x.data.push_back(x.data.at(p));
	m_pHash.insert(ParameterKeyPair(paraName, x));
}

void CParameterList::SetVectorData(string key, vector<double> value)
{
	g_paramLock.Enter();

	// Find the key, value pair associated with the given key.
	ParameterIterator i = m_pHash.find(key);

	// Set the key value if we found the key pair.
	if (i != m_pHash.end()) {
		i->second.data = value;
	}

	g_paramLock.Leave();
}

vector<double> CParameterList::GetVectorData(string key)
{
	g_paramLock.Enter();

	vector<double> value;

	// Try to find the pair associated with the given key.
	ParameterIterator i = m_pHash.find(key.c_str());

	// If we found an entry associated with the key, store the data
	// vector associated with it.
	if (i != m_pHash.end()) {
		value = i->second.data;
	}

	g_paramLock.Leave();

	return value;
}

string CParameterList::GetParamDescription(string param)
{
	g_paramLock.Enter();

	string s = "";

	// Find the parameter iterator.
	ParameterIterator i = m_pHash.find(param);

	if (i == m_pHash.end()) {
		s = "No Data Found";
	}
	else {
		s = i->second.description;
	}

	g_paramLock.Leave();

	return s;
}

string * CParameterList::GetKeyList(int &keyCount)
{
	g_paramLock.Enter();

	string *keyList;
	int i;

	// Number of elements in the hash.
	keyCount = m_pHash.size();

	// Initialize the key list.
	keyList = new string[keyCount];

	// Iterate through the hash and extract all the key names.
	ParameterIterator x;
	i = 0;
	for (x = m_pHash.begin(); x != m_pHash.end(); x++) {
		keyList[i] = x->first;
		i++;
	}

	g_paramLock.Leave();

	return keyList;
}

int CParameterList::GetListSize() const
{
	g_paramLock.Enter();
	int hashSize = m_pHash.size();
	g_paramLock.Leave();

	return hashSize;
}


bool CParameterList::IsVariable(string param)
{
	bool isVariable = false;

	g_paramLock.Enter();

	// Try to find the pair associated with the given key.
	ParameterIterator i = m_pHash.find(param.c_str());

	if (i != m_pHash.end()) {
		isVariable = i->second.variable;
	}

	g_paramLock.Leave();

	return isVariable;
}

bool CParameterList::Exists(string key)
{
	bool keyExists = false;

	g_paramLock.Enter();

	// Try to find the pair associated with the given key.
	ParameterIterator i = m_pHash.find(key.c_str());

	if (i != m_pHash.end()) {
		keyExists = true;
	}

	g_paramLock.Leave();

	return keyExists;
}

int CParameterList::GetParamSize(string param)
{
	g_paramLock.Enter();

	int paramSize = 0;

	// Try to find the pair associated with the given key.
	ParameterIterator i = m_pHash.find(param.c_str());

	if (i != m_pHash.end()) {
		paramSize = static_cast<int>(i->second.data.size());
	}

	g_paramLock.Leave();

	return paramSize;
}