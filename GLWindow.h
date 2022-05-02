#pragma once

#include "GlobalDefs.h"
#include "ParameterList.h"
#include "GLObject.h"
#include "Affine4Matrix.h"

#define USE_ANTIALIASING 1		// 1 = use anti-aliasing, 0 = don't
#define FP_DOTSIZE 5			// Fixation point size

#define DRAW_FP 0
#define DRAW_TARG1 1
#define DRAW_TARG2 2
#define DRAW_TARG_SLICES 100
#define SHAPE_ELLIPSE	0
#define SHAPE_RECTANGLE	1
#define SHAPE_CROSS	2

//For BINOC_DISP define
#define BINOC_DISP_BOTH_EYE 1
#define BINOC_DISP_LEFT_EYE 2
#define BINOC_DISP_RIGHT_EYE 3

using namespace std;

class GLPanel;

// Defines a stereo glFrustum.
typedef struct FRUSTUM_STRUCT
{
	GLfloat screenWidth,		// Width of the screen.
		    screenHeight,		// Height of the screen.
			camera2screenDist,	// Distance from the camera to the screen.
			clipNear,			// Distance from camera to near clipping plane.
			clipFar,			// Distance from camera to far clipping plane.
			eyeSeparation,		// Interocular distance
			worldOffsetX,		// Shifts the entire world horizontally.
			worldOffsetZ;		// Shifts the entire world vertically.
} Frustum;

// Defines a 3D field of stars.
typedef struct STARFIELD_STRUCT
{
	vector<double> dimensions,				// Width, height, depth dimensions of starfield.
				   triangle_size,			// Base width, height for each star.
				   fixationPointLocation,	// (x,y,z) origin of fixation point.
				   targ1Location,			// (x,y,z) origin of target 1.
				   targ2Location,			// (x,y,z) origin of target 2.
				   targLumMult,				// luminous of fixationPoint, targ1 and targ2 
				   starLeftColor,			// (r,g,b) value of left eye starfield.
				   starRightColor,			// (r,g,b) value of right eye starfield.
				   targXsize,				// X-dimension(cm) of FP & targets // following add by Johnny - 11/6/08
				   targYsize,				// Y-dimension(cm) of FP & targets
				   targShape,				// shape of FP & targets: ELLIPSE or RECTANGLE
				   targRlum,				// red luminance of targets/FP: 0 -> 1
				   targGlum,				// green luminance of targets/FP: 0 -> 1
				   targBlum;				// blue luminance of targets/FP: 0 -> 1
	//GLfloat targVertex[3][DRAW_TARG_SLICES*3];
	double density,
		   drawTarget,
		   drawFixationPoint,
		   drawTarget1,
		   drawTarget2,
		   drawBackground,
		   targetSize,
		   luminance,
		   probability,
		   use_lifetime,
		   cutoutRadius,
		   cutoutTextureLineWidth,
		   cutoutTextureSeparation,
		   cutoutTextureRotAngle;
	//int totalStars,
	int	lifetime,
		drawMode;
	bool useCutout,
		 useCutoutTexture;
} StarField;

// Represents a single star.
typedef struct STAR_STRUCT
{
	// Defines the 3 vertexes necessary to form a triangle.
	GLdouble x[3], y[3], z[3];
} Star;

// For MotionParallax, it also use Star struct to draw a point, so it only need
// one 3D position. All the code that copy from the MotionParallax will use 
// MoogDots' Star struct. For simplification, here we use x[3] instead of xyz[3]!
// Represents a single dot.  Defines the (x,y,z) location of the dot in space.
//typedef struct STAR_STRUCT
//{
//	GLdouble xyz[3];
//} Star;

// This is for mask infront of eye, so that it cover partial screen.
typedef struct EYE_MASK_STRUCT
{
	// Maximum will use 4 retangular to cover infront of eye.
	GLdouble quadsMaskVertex3D[4*4*3]; //4 quads with 4 vertex (xyz) 
	GLint count;
} EyeMask;

// Defines a window that contains a wxGLCanvas to display OpenGL stuff.
class GLWindow : public wxFrame
{
private:
	GLPanel *m_glpanel;
	int m_clientX, m_clientY;

public:
	GLWindow(const wxChar *title, int xpos, int ypos, int width, int height,
			 Frustum frustum, StarField starfield);

	// Returns a pointer to the embedded wxGLCanvas.
	GLPanel * GetGLPanel() const;
};


class GLPanel : public wxGLCanvas
{
private:
	Frustum m_frustum;			// Defines the frustum used in glFrustum().
	StarField m_starfield;		// Defines the starfield which will be rendered.
	GLfloat m_Heave,
			m_Surge,
			m_Lateral;
	Star *m_starArray;
	int m_frameCount,
		m_rotationType;
	nm3DDatum m_rotationVector;
	double m_rotationAngle,
		   m_centerX,
		   m_centerY,
		   m_centerZ;
	bool m_doRotation;
	GLuint m_starFieldCallList;
	GLuint m_staticObjCallList[2]; // for drawing static objects
	int m_screenWidth;			//glPanel screen width
	int m_screenHeight;			//glPanel screen height
	Affine4Matrix m_rotCameraCoordMatrix[2];
	EyeMask m_eyeMask[2];
	GLfloat targVertex[3][DRAW_TARG_SLICES*3];
	int m_totalStars;
	bool computeStarField; //Only true when DO_MOVEMENT == -1, otherwise will be false. -Johnny 4/19/2011

public:
	GLPanel(wxWindow *parent, int width, int height, Frustum frustum, StarField starfield, int *attribList);
	~GLPanel();

	void OnPaint(wxPaintEvent &event);
	void OnSize(wxSizeEvent &event);
	void SetHeave(GLdouble heave);
	void SetLateral(GLdouble lateral);
	void SetSurge(GLdouble surge);
	void SetComputeStarField(bool compute){computeStarField = compute;}
	bool GetComputeStarField(){return computeStarField;}

	// This is the main function that draws the OpenGL scene.
	GLvoid Render();

	// Does any one time OpenGL initializations.
	GLvoid InitGL();

	// Gets/Sets the frustum for the GL scene.
	Frustum * GetFrustum();
	GLvoid SetFrustum(Frustum frustum);

	// Gets/Sets the starfield data for the GL scene and recreates the
	// individual star information.
	StarField * GetStarField();
	GLvoid SetStarField(StarField starfield);

	// Sets the rotation vector.
	void SetRotationVector(nm3DDatum rotationVector);

	// Sets the rotation angle in degrees.
	void SetRotationAngle(double angle);

	// Sets rotation camera coord matrix
	void SetRotCameraCoordMatrix(int whichEye, Affine4Matrix rotCameraCoordMatrix);

	// Sets eye mask
	void SetEyeMask(int whichEye, EyeMask eyeMask);

	// Sets whether or not we should do any rotation.
	void DoRotation(bool val);

	// Sets whether or not we rotate the fixation point, the background, or both.
	// val = 0, rotate the background, but not the fixation point.
	// val = 1, rotate both the background and fixation point.
	// val = 2, rotate the fixation point, but not the background.
	void RotationType(int val);

	// Sets the center of rotation.
	void SetRotationCenter(double x, double y, double z);

	// Calcultates the glFrustum for a stereo scene.
	GLvoid CalculateStereoFrustum(GLfloat screenWidth, GLfloat screenHeight, GLfloat camera2screenDist,
								  GLfloat clipNear, GLfloat clipFar, GLfloat eyeSeparation,
								  GLfloat centerOffsetX, GLfloat centerOffsetY);
private:
	// Generates the starfield.
	GLvoid GenerateStarField();

	// Generates the object starfield texture.
	GLvoid GenerateAllObjectStarFieldTexture();

	// Draws the generated starfield.
	GLvoid DrawStarField();

	// Used to alter star locations due to their lifetimes.
	GLvoid ModifyStarField();

	// Used to alter star locations of objects due to their lifetimes.
	GLvoid ModifyAllObjectStarField();

	// Create a glCallList for hollow sphere dots(circle)
	GLvoid SetupCallList();

	// Rendering all Objects depend on which eye
	GLvoid RenderAllObj(int whichEye);

	// Setup glCallList for all static objects
	GLvoid SetupStaticObjCallList(int whichEye);
	
	// Rendering FP & targets
	GLvoid DrawTargetObject(int targObj);

	// Rendering object targets
	GLvoid DrawObjectTargets(Point3 cameraTranslate);

	//Function to compute a random floating point # in the range from min->max 
	float RandomFloat(float min, float max);

	// draw eye masks
	GLvoid DrawEyeMask(int whichEye);

	// draw cutoff screen (use bit 1 mask)
	GLvoid DrawCutoff();

	// draw stimulus mask (use bit 2 mask)
	GLvoid DrawStimMask();

	GLvoid Project2DPlaneToCylinder(Star *dotarray, GLint dot_num, double pref_direction, double eye_shift, double view_dist,
								double dot_depth_signal, int bin_corr_crit, double dot_depth_min, double dot_depth_max);
	GLvoid CutOutBGDotsByCylinder(GLfloat *dotarray, GLint dot_num, double pref_direction, double eye_shift, double view_dist,
								double dot_depth_signal, int bin_corr_crit, double dot_depth_min, double dot_depth_max,
								double rf_x, double rf_y, double rf_dia);

public:
	// for drawing calibration line of min and max rotation angle
	double minPredictedAngle, maxPredictedAngle;
	double lineWidth, lineLength, squareSize, squareCenter[2];
	int calibTranOn[5], calibRotOn[2], calibRotMotion, calibSquareOn;

	GLfloat *starFieldVertex3D, *worldRefVertex3D; // for glDrawArrays() to speed up the drawing
	vector <int> dotSizeDistribute;
	int totalDotsNum;
	vector <double> aziVector, eleVector;
	Point3 currCameraTrans;

	//When star feild or object start moving, we will change the rendering context,
	// wglMakeCurrent((HDC)m_moogCom->glPanel->GetContext()->GetHDC(), m_threadGLContext);
	// SwapBuffers((HDC)m_moogCom->glPanel->GetContext()->GetHDC());
	//so, we need redo all the glCallList or texture stuff after wglMakeCurrent().
	bool redrawCallList;		
	bool redrawObjTexture;

	double FP_cross[6];			//for 3D Fixation Cross
	double hollowSphereRadius;	
	vector<double> stimMaskVars;//stencil mask for drawing stimulus inside 
	GLObject *glDynObject[NUM_OF_DYN_OBJ];			// Define the drawing part of the mulit dynamic objects
	GLObject *glStaticObject[NUM_OF_STATIC_OBJ];	// Define the drawing part of the mulit static objects

	//Setup input parameters
	void SetupParameters();		  
	//Get particular dynamic object's star field parameters
	ObjStarField GetDynObjStarField(int objNum);
	//Get particular static object's star field parameters
	ObjStarField GetStaticObjStarField(int objNum);
	//Control the object translation movement
	void SetObjFieldTran(int objNum, double x, double y, double z);
	//Control the object rotation angle according the rotation vector that set by user
	void SetAllObjRotAngle(int numFrame);
	//Get glPanel screen width
	double GetScreenWidth(){return m_screenWidth;}	
	//Get glPanel screen height
	double GetScreenHeigth(){return m_screenHeight;}
	//Save the position of pixel that project on screen. For checking error.
	nmMovementData winPointTraj[2];
	
	/***************   Motion Parallax stuff  *******************/
private:
	Star *m_backgroundArray;		// All star locations for the background dots.
	GLint m_background_num;			// Number of background dots or 0 if background is off.
	GLdouble m_background_dens;		// Density of background dots or 0 if background is off.
	GLdouble m_background_x_size, m_background_y_size, m_background_depth;	// Size and depth of background plane.
	GLdouble m_background_seed;		// random seed for background
	Star *m_dotArray;						// All star locations for patch dots.
	Star *m_dotArray_init;					// All star locations for saving initial dots.
	GLint m_dot_num;						// Number of patch dots.
	GLdouble m_dot_dens;					// Density of patch dots.
	GLdouble m_dot_d_size, m_dot_depth,		// Size (diameter) of dot patch and depth of dot patch.
			 m_dot_depth_min, m_dot_depth_max, // range of depth in randomization (coherence)
			 m_dot_bin_corr,				// binocular correlation
			 m_dot_x, m_dot_y;				// Location of dot patch center in (x,y).
	GLdouble m_fpx, m_fpy;	// Fixation point location.
	//GLfloat m_Heave,
	//		m_Surge,
	//		m_Lateral;
	GLdouble m_pref_direction;	// Preferred direction.
	GLdouble m_binoc_disp;
	GLdouble m_dot_seed;		// random seed for dot
	// GLdouble m_VM_angle;  // Angle (in radians) on VM circle subtended by the patch.  Was to be used by BD condition.  Not used.
	//int m_frameCount;
	int m_stimOn;			//1/0=On/Off visual stimulus.

public:
	GLvoid SetStimParams();

	// Calculate both background and patch dot numbers from specified dot densities.
	GLvoid GenerateDotDensities();

	GLvoid RenderMotionParallax();

	// Conversion macros. - We don't use VIEW_DIST any more, so create functions for them. - Johnny 12/15/08
	//#define CMPERDEG  g_pList.GetVectorData("VIEW_DIST").at(0)*PI/180.0
	//#define DISPTODEPTH(disp)  (g_pList.GetVectorData("VIEW_DIST").at(0)-((180.0*g_pList.GetVectorData("IO_DIST").at(0)*g_pList.GetVectorData("VIEW_DIST").at(0))/(-disp*PI*g_pList.GetVectorData("VIEW_DIST").at(0)+180.0*g_pList.GetVectorData("IO_DIST").at(0))))
	GLdouble CmPerDeg();
	double DISPTODEPTH(double disp);

	vector<double> ConvertObjOriginFromDegToMeter(vector<double> degCoord);
	vector<double> ConvertObjVolumeFromDegToMeter(vector<double> volumeDegCoord, vector<double> originMeterCoord);
	double CalObjDensityFromDegToCm(ObjStarField objfield);

	GLvoid SetStimOn(int stimOn){m_stimOn=stimOn;};

	//Using two point form formula to calculate a point that given depth(z) at a straight line that formed by two points.
	Point3 FindPointByTwoPointForm(Point3 a, Point3 b, double depthZ);

private:
	// Generates the background plane of dots.
	GLvoid GenerateBackgroundDotField();

	// Generates a circular plane of dots.
	GLvoid GenerateDotField();
	GLvoid GenerateDotField2();
	GLvoid DrawIncongDotFields();

	// Draws both dot fields.
	GLvoid DrawDotFields();
	// Draw fixation for motion parallax condition
	GLvoid DrawFixation();

	// Draw cutout texture
	GLvoid DrawCutoutTexture();

	// Draw cross for any position
	GLvoid DrawCross(double x, double y, double z, double lineLength, double lineWidth, int whichEye);

	// Generates the world reference object.
	GLvoid GenerateWorldRefObj();

	// Draws the world reference object.
	GLvoid DrawWorldRefObj(int whichEye);

private:
	DECLARE_EVENT_TABLE()
};

#include "GLWindow.inl"
