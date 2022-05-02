#include "StdAfx.h"
#include "GLWindow.h"

// Parameter list -- Original declaration can be found in ParameterList.cpp
extern CParameterList g_pList;

extern int RIG_ROOM;
extern int SCREEN_WIDTH;
extern int SCREEN_HEIGHT;
extern float CENTER2SCREEN;
extern int PROJECTOR_NATIVE_WIDTH;		//in pixels
extern int PROJECTOR_NATIVE_HEIGHT;

/****************************************************************************************/
/*	GLWindow Definitions ****************************************************************/
/****************************************************************************************/
GLWindow::GLWindow(const wxChar *title, int xpos, int ypos, int width, int height,
				   Frustum frustum, StarField starfield) :
			  wxFrame((wxFrame *) NULL, -1, title, wxPoint(xpos, ypos), wxSize(width, height), wxSIMPLE_BORDER)
{
	GetClientSize(&m_clientX, &m_clientY);

	// Setup the pixel format descriptor.
#if USE_STEREO
	int attribList[6];
	attribList[0] = WX_GL_STEREO;
	attribList[1] = WX_GL_DOUBLEBUFFER;
	attribList[2] = WX_GL_RGBA;
	attribList[3] = WX_GL_STENCIL_SIZE; attribList[4] = 8;
	attribList[5] = 0;
#else
	int attribList[5];
	attribList[0] = WX_GL_DOUBLEBUFFER;
	attribList[1] = WX_GL_RGBA;
	attribList[2] = WX_GL_STENCIL_SIZE; attribList[3] = 8;
	attribList[4] = 0;
#endif

	// Create the embedded panel where all the OpenGL stuff will be shown.
	m_glpanel = new GLPanel(this, m_clientX, m_clientY, frustum, starfield, attribList);
	m_glpanel->SetBackgroundColour(wxColor(0,0,0));
}


/****************************************************************************************/
/*	GLPanel Definitions *****************************************************************/
/****************************************************************************************/
BEGIN_EVENT_TABLE(GLPanel, wxGLCanvas)
EVT_PAINT(GLPanel::OnPaint)
EVT_SIZE(GLPanel::OnSize)
END_EVENT_TABLE()


GLPanel::GLPanel(wxWindow *parent, int width, int height, Frustum frustum, StarField starfield, int *attribList) :
		 wxGLCanvas(parent, -1, wxPoint(0, 0), wxSize(width, height), 0, "GLCanvas", attribList),
		 m_frustum(frustum), m_starfield(starfield), m_Heave(0.0), m_Surge(0.0), m_Lateral(0.0),
		 m_starArray(NULL), m_frameCount(1), redrawCallList(true), starFieldVertex3D(NULL),// objectFieldVertex3D(NULL),
		 m_backgroundArray(NULL), m_dotArray(NULL), m_dotArray_init(NULL), m_dot_dens(0.0), m_dot_depth(0.0),
		 m_screenWidth(width), m_screenHeight(height)
{
	m_starFieldCallList = glGenLists(1);
	m_staticObjCallList[0] = glGenLists(1);
	m_staticObjCallList[1] = glGenLists(1);
	worldRefVertex3D = new GLfloat[8*3*2*3]; //8 corners * 3 lines * 2 points * 3 coords;

	for(int i=0; i<NUM_OF_DYN_OBJ; i++)
		glDynObject[i] = new GLObject(this, GetDynObjStarField(i));

	for(int i=0; i<NUM_OF_STATIC_OBJ; i++)
		glStaticObject[i] = new GLObject(this, GetStaticObjStarField(i));

	SetupParameters();

	InitGL();
	GenerateStarField();

	//Set default values.
	m_rotationAngle = 0.0;
	m_rotationVector.x = 0.0; m_rotationVector.y = 0.0; m_rotationVector.z = 0.0;
	m_doRotation = false;
	m_rotationType = 0;
	minPredictedAngle = 0.0; maxPredictedAngle = 0.0;
	redrawCallList = false;
	redrawObjTexture = false;
	computeStarField = false;
}

void GLPanel::SetupParameters()
{
	lineWidth = g_pList.GetVectorData("CALIB_LINE_SETUP").at(0);
	lineLength = g_pList.GetVectorData("CALIB_LINE_SETUP").at(1);
	calibTranOn[0]=g_pList.GetVectorData("CALIB_TRAN_ON").at(0); 
	calibTranOn[1]=g_pList.GetVectorData("CALIB_TRAN_ON").at(1); 
	calibTranOn[2]=g_pList.GetVectorData("CALIB_TRAN_ON").at(2); 
	calibTranOn[3]=g_pList.GetVectorData("CALIB_TRAN_ON").at(3); 
	calibTranOn[4]=g_pList.GetVectorData("CALIB_TRAN_ON").at(4); 
	calibRotOn[0]=g_pList.GetVectorData("CALIB_ROT_ON").at(0); 
	calibRotOn[1]=g_pList.GetVectorData("CALIB_ROT_ON").at(1);
	calibRotMotion=g_pList.GetVectorData("CALIB_ROT_MOTION").at(0);
	calibSquareOn = g_pList.GetVectorData("CALIB_SQUARE").at(0);
	squareSize = g_pList.GetVectorData("CALIB_SQUARE").at(1); //cm
	squareCenter[0] = g_pList.GetVectorData("CALIB_SQUARE").at(2);  
	squareCenter[1] = g_pList.GetVectorData("CALIB_SQUARE").at(3); 
	FP_cross[0] = g_pList.GetVectorData("FP_CROSS").at(0); 
	FP_cross[1] = g_pList.GetVectorData("FP_CROSS").at(1); 
	FP_cross[2] = g_pList.GetVectorData("FP_CROSS").at(2); 
	FP_cross[3] = g_pList.GetVectorData("FP_CROSS").at(3); 
	FP_cross[4] = g_pList.GetVectorData("FP_CROSS").at(4); 
	FP_cross[5] = g_pList.GetVectorData("FP_CROSS").at(5); 
	hollowSphereRadius = g_pList.GetVectorData("DOTS_SPHERE_PARA").at(0);
	stimMaskVars = g_pList.GetVectorData("STIMULUS_MASK");
	if(stimMaskVars.at(3) == 1.0) //use degree
	{	// change unit from degree to cm
		stimMaskVars.at(0) = 2.0*m_frustum.camera2screenDist*tan((stimMaskVars.at(0)/2.0)*PI/180.0); // horizontal
		stimMaskVars.at(1) = 2.0*m_frustum.camera2screenDist*tan((stimMaskVars.at(1)/2.0)*PI/180.0); // vertical
		stimMaskVars.at(2) = 2.0*m_frustum.camera2screenDist*tan((stimMaskVars.at(2)/2.0)*PI/180.0); // radius
	}

	// find vertex of ellipse shape and store in vector
	for(int i=0; i<3; i++)
	{
		int j=0;
		for(int k=0; k<DRAW_TARG_SLICES; k++)
		{
			targVertex[i][j++] = m_starfield.targXsize.at(i)*cos(k*2*PI/DRAW_TARG_SLICES)/2.0;
			targVertex[i][j++] = m_starfield.targYsize.at(i)*sin(k*2*PI/DRAW_TARG_SLICES)/2.0;
			targVertex[i][j++] = 0;
		}
	}

	// setup dynamic object parameters
	for(int i=0; i<NUM_OF_DYN_OBJ; i++)
		glDynObject[i]->SetupObject(GetDynObjStarField(i), computeStarField);
	// setup static object parameters
	for(int i=0; i<NUM_OF_STATIC_OBJ; i++)
		glStaticObject[i]->SetupObject(GetStaticObjStarField(i), computeStarField);
	// setup glList for static object parameters

	//Generate world reference object
	GenerateWorldRefObj();
}

ObjStarField GLPanel::GetDynObjStarField(int objNum)
{
	vector<double> tmpVector, copyObjVector;
	ObjStarField objfield;
	int i = objNum;

	// need use tmpVector; directly using g_pList.GetVectorData("OBJ_VOLUME") will have memory leak problem.
	// 4 parameters per object
	tmpVector = g_pList.GetVectorData("OBJ_M_ORIGIN"); //(x,y,z,w) where w=0 uses meter and w=1 uses degree.
	objfield.origin.assign(tmpVector.begin()+i*4, tmpVector.begin()+(i+1)*4-1); //(x,y.z)
	if(tmpVector.at((i+1)*4-1)==1.0 && g_pList.GetVectorData("IO_DIST").at(0)!=0.0) //if w==1, origin uses degree
		objfield.origin = ConvertObjOriginFromDegToMeter(objfield.origin);
	tmpVector = g_pList.GetVectorData("OBJ_VOLUME"); //(w,h,d,cm/deg)
	objfield.dimensions.assign(tmpVector.begin()+i*4, tmpVector.begin()+(i+1)*4-1); //(w,h,d)
	if(tmpVector.at((i+1)*4-1)==1.0) //if w==1, volume uses degree
		objfield.dimensions = ConvertObjVolumeFromDegToMeter(objfield.dimensions, objfield.origin);
	// 3 parameters per object
	//tmpVector = g_pList.GetVectorData("OBJ_VOLUME"); 
	//objfield.dimensions.assign(tmpVector.begin()+i*3, tmpVector.begin()+(i+1)*3);
	tmpVector = g_pList.GetVectorData("OBJ_LEYE_COLOR"); 
	objfield.starLeftColor.assign(tmpVector.begin()+i*3, tmpVector.begin()+(i+1)*3);
	tmpVector = g_pList.GetVectorData("OBJ_REYE_COLOR"); 
	objfield.starRightColor.assign(tmpVector.begin()+i*3, tmpVector.begin()+(i+1)*3);
	tmpVector = g_pList.GetVectorData("OBJ_STAR_PATTERN"); 
	objfield.starPattern.assign(tmpVector.begin()+i*3, tmpVector.begin()+(i+1)*3);
	// 2 parameters per object
	tmpVector = g_pList.GetVectorData("OBJ_STAR_SIZE");
	objfield.triangle_size.assign(tmpVector.begin()+i*2, tmpVector.begin()+(i+1)*2);
	objfield.textureSegment.assign(2,32);
	objfield.rotAzi = g_pList.GetVectorData("OBJ_ROT_VECTOR").at(i*2);
	objfield.rotEle = g_pList.GetVectorData("OBJ_ROT_VECTOR").at(i*2+1);
	objfield.density = g_pList.GetVectorData("OBJ_STAR_DENSITY").at(i*2);
	objfield.densityType = g_pList.GetVectorData("OBJ_STAR_DENSITY").at(i*2+1);
	// 1 parameter per object
	//objfield.density = g_pList.GetVectorData("OBJ_STAR_DENSITY").at(i);
	objfield.enable = g_pList.GetVectorData("OBJ_ENABLE").at(i);
	objfield.luminance = g_pList.GetVectorData("OBJ_LUM_MULT").at(i);
	objfield.luminanceFlag = g_pList.GetVectorData("OBJ_DYN_LUM_FLAG").at(i);
	objfield.lifetime = (int)g_pList.GetVectorData("OBJ_LIFETIME").at(i);
	objfield.probability = g_pList.GetVectorData("OBJ_MOTION_COHERENCE").at(i);
	objfield.use_lifetime = g_pList.GetVectorData("OBJ_LIFETIME_ON").at(i);
	objfield.shape = g_pList.GetVectorData("OBJ_SHAPE").at(i);
	objfield.starType = g_pList.GetVectorData("OBJ_STAR_TYPE").at(i);
	objfield.dotSize = g_pList.GetVectorData("OBJ_DOTS_SIZE").at(i);
	objfield.starDistrubute = g_pList.GetVectorData("OBJ_STAR_DISTRIBUTE").at(i);
	objfield.bodyType = g_pList.GetVectorData("OBJ_BODY_TYPE").at(i);
	objfield.initRotAngle = g_pList.GetVectorData("OBJ_INIT_ROT").at(i);
	objfield.rotSpeed = g_pList.GetVectorData("OBJ_ROT_SPEED").at(i);
	objfield.numOfCopy = g_pList.GetVectorData("OBJ_NUM_OF_COPY").at(i);
	if(objfield.numOfCopy>MAX_DYN_OBJ_COPY_NUM) objfield.numOfCopy=MAX_DYN_OBJ_COPY_NUM;

	//For copy object translated distance in xyz
	tmpVector = g_pList.GetVectorData("OBJ_COPY_TRANS"); 
	copyObjVector.assign(tmpVector.begin()+i*3*MAX_DYN_OBJ_COPY_NUM, tmpVector.begin()+(i+1)*3*MAX_DYN_OBJ_COPY_NUM);
	int j = 0;
	XYZ3 tranDis;
	for(int ic=0; ic<objfield.numOfCopy; ic++){
		tranDis.x = copyObjVector.at(j++);
		tranDis.y = copyObjVector.at(j++);
		tranDis.z = copyObjVector.at(j++);
		objfield.copyObjTrans.push_back(tranDis);
	}

	if(objfield.densityType==1.0) // unit in degree
	{	//We only change density from degree to cm in 2D case(volumn depth=0)
		if(objfield.dimensions.at(2)==0.0) objfield.density = CalObjDensityFromDegToCm(objfield);
		else objfield.density = 0.0; // We set density=0 and warn people should not use density in degree for 3D volume case.
	}

	return objfield;
}

ObjStarField GLPanel::GetStaticObjStarField(int objNum)
{
	vector<double> tmpVector;
	ObjStarField objfield;
	int i = objNum;

	// 4 parameters per object
	tmpVector = g_pList.GetVectorData("STATIC_OBJ_M_ORIGIN"); //(x,y,z,w) where w=0 uses meter and w=1 uses degree.
	objfield.origin.assign(tmpVector.begin()+i*4, tmpVector.begin()+(i+1)*4-1); //(x,y.z)
	if(tmpVector.at((i+1)*4-1)==1.0 && g_pList.GetVectorData("IO_DIST").at(0)!=0.0) //if w==1, origin uses degree
		objfield.origin = ConvertObjOriginFromDegToMeter(objfield.origin);
	tmpVector = g_pList.GetVectorData("STATIC_OBJ_VOLUME"); //(w,h,d,cm/deg)
	objfield.dimensions.assign(tmpVector.begin()+i*4, tmpVector.begin()+(i+1)*4-1); //(w,h,d)
	if(tmpVector.at((i+1)*4-1)==1.0) //if w==1, volume uses degree
		objfield.dimensions = ConvertObjVolumeFromDegToMeter(objfield.dimensions, objfield.origin);
	// 3 parameters per object
	//tmpVector = g_pList.GetVectorData("STATIC_OBJ_VOLUME"); // need use tmpVector; directly using g_pList.GetVectorData("OBJ_VOLUME") will have memory leak problem.
	//objfield.dimensions.assign(tmpVector.begin()+i*3, tmpVector.begin()+(i+1)*3);
	tmpVector = g_pList.GetVectorData("STATIC_OBJ_LEYE_COLOR"); 
	objfield.starLeftColor.assign(tmpVector.begin()+i*3, tmpVector.begin()+(i+1)*3);
	tmpVector = g_pList.GetVectorData("STATIC_OBJ_REYE_COLOR"); 
	objfield.starRightColor.assign(tmpVector.begin()+i*3, tmpVector.begin()+(i+1)*3);
	tmpVector = g_pList.GetVectorData("STATIC_OBJ_STAR_PATTERN"); 
	objfield.starPattern.assign(tmpVector.begin()+i*3, tmpVector.begin()+(i+1)*3);
	// 2 parameters per object
	tmpVector = g_pList.GetVectorData("STATIC_OBJ_STAR_SIZE");
	objfield.triangle_size.assign(tmpVector.begin()+i*2, tmpVector.begin()+(i+1)*2);
	objfield.textureSegment.assign(2,32);
	objfield.rotAzi = g_pList.GetVectorData("STATIC_OBJ_ROT_VECTOR").at(i*2);
	objfield.rotEle = g_pList.GetVectorData("STATIC_OBJ_ROT_VECTOR").at(i*2+1);
	objfield.density = g_pList.GetVectorData("STATIC_OBJ_STAR_DENSITY").at(i*2);
	objfield.densityType = g_pList.GetVectorData("STATIC_OBJ_STAR_DENSITY").at(i*2+1);
	// 1 parameter per object
	//objfield.density = g_pList.GetVectorData("STATIC_OBJ_STAR_DENSITY").at(i);
	objfield.enable = g_pList.GetVectorData("STATIC_OBJ_ENABLE").at(i);
	objfield.luminance = g_pList.GetVectorData("STATIC_OBJ_LUM_MULT").at(i);
	objfield.lifetime = 5.0;
	objfield.probability = 100.0;
	objfield.use_lifetime = 0.0;
	objfield.shape = g_pList.GetVectorData("STATIC_OBJ_SHAPE").at(i);
	objfield.starType = g_pList.GetVectorData("STATIC_OBJ_STAR_TYPE").at(i);
	objfield.dotSize = g_pList.GetVectorData("STATIC_OBJ_DOTS_SIZE").at(i);
	objfield.starDistrubute = g_pList.GetVectorData("STATIC_OBJ_STAR_DISTRIBUTE").at(i);
	objfield.bodyType = g_pList.GetVectorData("STATIC_OBJ_BODY_TYPE").at(i);
	objfield.initRotAngle = g_pList.GetVectorData("STATIC_OBJ_INIT_ROT").at(i);
	objfield.rotSpeed = 0.0;
	objfield.numOfCopy = 0.0;

	if(objfield.densityType==1.0) // unit in degree
	{	//We only change density from degree to cm in 2D case(volumn depth=0)
		if(objfield.dimensions.at(2)==0.0) objfield.density = CalObjDensityFromDegToCm(objfield);
		else objfield.density = 0.0; // We set density=0 and warn people should not use density in degree for 3D volume case.
	}

	return objfield;
}

vector<double> GLPanel::ConvertObjOriginFromDegToMeter(vector<double> degCoord)
{
	vector<double> meterCoord;
	double depth = DISPTODEPTH(degCoord.at(2)); //z-coord in cm that matches the OpenGL coord where screen center at (0,0,0).
	//use equation x=z*tan(angle) that assume the center eye at z-coord
	meterCoord.push_back((m_frustum.camera2screenDist-depth)*tan(degCoord.at(0)*PI/180)/100.0); //x-coord in meter
	meterCoord.push_back((m_frustum.camera2screenDist-depth)*tan(degCoord.at(1)*PI/180)/100.0); //y-coord in meter
	meterCoord.push_back(depth/100.0); //z-coord in meter;

	return meterCoord;
}

vector<double> GLPanel::ConvertObjVolumeFromDegToMeter(vector<double> volumeDegCoord, vector<double> originMeterCoord)
{
	vector<double> cmCoord;
	double camera2obj = m_frustum.camera2screenDist-originMeterCoord.at(2)*100; //cm
	//Assume the center of eye and object at z-coord 
	cmCoord.push_back(2.0*camera2obj*tan((volumeDegCoord.at(0)/2.0)*PI/180.0));
	cmCoord.push_back(2.0*camera2obj*tan((volumeDegCoord.at(1)/2.0)*PI/180.0));
	cmCoord.push_back(2.0*camera2obj*tan((volumeDegCoord.at(2)/2.0)*PI/180.0));

	return cmCoord;
}

double GLPanel::CalObjDensityFromDegToCm(ObjStarField objfield)
{
	//This calculate the density per cm^2
	double camera2obj = m_frustum.camera2screenDist-objfield.origin.at(2)*100; //cm
	return objfield.density/( (camera2obj*tan(PI/180)) * (camera2obj*tan(PI/180)) );//suppose the volume z==0;
}

GLPanel::~GLPanel()
{
	delete m_starArray;

	for(int i=0; i<NUM_OF_DYN_OBJ; i++)
		delete glDynObject[i];
	for(int i=0; i<NUM_OF_STATIC_OBJ; i++)
		delete glStaticObject[i];

	//MotionParallax stuff
	// Delete the old Star array if needed.
	if (m_backgroundArray != NULL) {
		delete [] m_backgroundArray;
	}

	// Delete the old Star array if needed.
	if(m_dotArray != NULL) delete [] m_dotArray;
	if(m_dotArray_init != NULL) delete [] m_dotArray_init;

	if(worldRefVertex3D != NULL){
		delete [] worldRefVertex3D;
	}

}

GLvoid GLPanel::DrawFixation()
{
	//double fpsize = CMPERDEG*g_pList.GetVectorData("FP_SIZE").at(0)/2.0;
	double fpsize = CmPerDeg()*g_pList.GetVectorData("FP_SIZE").at(0)/2.0;

	glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
	// Draw fixation cross
	if(g_pList.GetVectorData("FP_ON").at(0) && fpsize>0.0)
	{    
		double FPgain = g_pList.GetVectorData("PURSUIT_GAIN").at(0);   // Compensates for poor pursuit gain.
		glBegin(GL_LINES);
			glVertex3f(fpsize+m_fpx-(m_Lateral/FPgain-m_Lateral), m_fpy+(m_Heave/FPgain-m_Heave),0.0);
			glVertex3f(-fpsize+m_fpx-(m_Lateral/FPgain-m_Lateral), m_fpy+(m_Heave/FPgain-m_Heave),0.0);
			glVertex3f(m_fpx-(m_Lateral/FPgain-m_Lateral), fpsize+m_fpy+(m_Heave/FPgain-m_Heave),0.0);
			glVertex3f(m_fpx-(m_Lateral/FPgain-m_Lateral), -fpsize+m_fpy+(m_Heave/FPgain-m_Heave),0.0);
		glEnd();
	}
}

GLvoid GLPanel::RenderMotionParallax()
{
	double r;
	GLdouble screen_width = g_pList.GetVectorData("SCREEN_DIMS").at(0),
			 screen_height = g_pList.GetVectorData("SCREEN_DIMS").at(1),
			 //view_dist = g_pList.GetVectorData("VIEW_DIST").at(0), 
			 //f.camera2screenDist = CENTER2SCREEN - eyeOffsets.at(1) - headCenter.at(1);	// Distance from monkey to screen.
			 view_dist = m_frustum.camera2screenDist,
			 clip_near = g_pList.GetVectorData("CLIP_PLANES").at(0), 
			 clip_far = g_pList.GetVectorData("CLIP_PLANES").at(1),
			 eye_separation = g_pList.GetVectorData("BINOC_DISP").at(0)==1.0?(g_pList.GetVectorData("IO_DIST").at(0)/2.0):0.0;  // Eye seperation only when we're doing binocular conditions!  All monocular conditions are now cyclopean.

	if(g_pList.GetVectorData("CLIP_PLANES").at(2)==1.0)// use degree
	{
		clip_near = view_dist - DISPTODEPTH(clip_near);
		clip_far = view_dist - DISPTODEPTH(clip_far);
	}

	// Viewport shifter for debugging purposes.
	//if(g_pList.GetVectorData("GLU_LOOK_AT").at(0)==0.0){
	//	GLdouble movetopixel = g_pList.GetVectorData("MOVETOPIXEL").at(0);
	//	glViewport(150+m_Lateral*movetopixel,50-m_Heave*movetopixel,500,500);
	//}
	//else{
	//	glViewport(150,50,500,500);
	//}
	// End Viewport shifter.

	// Reduced viewport to compensate for lens.  Hardcoded now in InitGL().  Use this for debugging.
	// int w = g_pList.GetVectorData("VIEWPORT").at(0),
	//     h = g_pList.GetVectorData("VIEWPORT").at(1);
	// int x = (1280 - w) / 2,
	//     y = (1024 - h) / 2;
	// glViewport(x-0, y, w, h);

	glPointSize((GLfloat)g_pList.GetVectorData("DOT_SIZE").at(0));
	//glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

	// TODO hard-coding dwell function. hkim. later implemented like BACKGROUND_DWELL
	//if(m_frameCount%15==0){
		//GenerateBackgroundDotField();
	//}

	if(m_frameCount++%2==0){
		if(g_pList.GetVectorData("BACKGROUND_COHERENCE").at(0)==0.0) GenerateBackgroundDotField();
		if(g_pList.GetVectorData("PATCH_COHERENCE").at(0)==0.0)	GenerateDotField();
	}

	////**** BACK LEFT BUFFER. ****//
#if USE_STEREO
	glDrawBuffer(GL_BACK_LEFT);
#else
	glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
#endif
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);		// Clears the current scene.

	//CalculateStereoFrustum(screen_width, screen_height, view_dist, clip_near, clip_far, -eye_separation, 0, 0);
	CalculateStereoFrustum(m_frustum.screenWidth, m_frustum.screenHeight, m_frustum.camera2screenDist,
						   m_frustum.clipNear, m_frustum.clipFar, -eye_separation,
						   m_frustum.worldOffsetX, m_frustum.worldOffsetZ);
	glMatrixMode(GL_MODELVIEW);

	glLoadIdentity();

	
	// Setup the camera.
	switch((int)g_pList.GetVectorData("GLU_LOOK_AT").at(0)){
		case 0:
			gluLookAt(-eye_separation+m_Lateral, 0.0f-m_Heave, view_dist-m_Surge,		// Camera origin
						-eye_separation+m_Lateral, 0.0f-m_Heave, view_dist-m_Surge-1.0f,	// Camera direction
						0.0, 1.0, 0.0);
			//gluLookAt(-eye_separation+0, 0.0f-GL_OFFSET, view_dist-0,		// Camera origin
			//		  -eye_separation+0, 0.0f-GL_OFFSET, view_dist-0-1.0f,	// Camera direction
			//		  0.0, 1.0, 0.0);
			break;
		case 1:
			gluLookAt(m_Lateral, 0.0f-m_Heave, view_dist-m_Surge,		// Camera origin
					    m_fpx, m_fpy, 0.0, 	// Camera direction
					    0.0, 1.0, 0.0);
			glTranslated(eye_separation,0.0,0.0);  // RM condition is cyclopean now; eye_separation should be zero.
			break;
		case 2:		// Not in use!  Only works for pref_direction = 0.
			r = view_dist/2.0;
			gluLookAt(sin(m_Lateral)*r, 0.0f-m_Heave, -cos(m_Lateral)*-r+r-m_Surge,		// Camera origin
				        m_fpx, m_fpy, 0.0, 	// Camera direction
					    0.0, 1.0, 0.0);
			glTranslated(eye_separation,0.0,0.0);
			break;
		case 3:
			gluLookAt(-eye_separation, 0.0, view_dist-m_Surge,			// Position straight ahead, don't move
						-eye_separation, 0.0, view_dist-m_Surge-1.0f,
						0.0, 1.0, 0.0);
	}

	//if(g_pList.GetVectorData("BINOC_DISP").at(0) < 3.0){  // Left eye should be drawn for all cases except BINOC_DISP == 3
	//// if(g_pList.GetVectorData("BINOC_DISP").at(0) == 1.0 || (g_pList.GetVectorData("BINOC_DISP").at(0) == 2.0 && g_pList.GetVectorData("PATCH_DISP_EYE").at(0) == 1.0)){  
	//	glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
	//	glColor3d(g_pList.GetVectorData("STAR_LEYE_COLOR").at(0),g_pList.GetVectorData("STAR_LEYE_COLOR").at(1),g_pList.GetVectorData("STAR_LEYE_COLOR").at(2));

	//	if((int)g_pList.GetVectorData("CTRL_STIM_ON").at(0) == 1)
	//	{
	//		 if(m_stimOn == 1) DrawDotFields();
	//	}
	//	else DrawDotFields();
	//}

	//// Draw the left starfield.
	//glColor3d(m_starfield.starLeftColor[0] * m_starfield.luminance,		// Red
	//		m_starfield.starLeftColor[1] * m_starfield.luminance,		// Green
	//		m_starfield.starLeftColor[2] * m_starfield.luminance);	// Blue
	//if (g_pList.GetVectorData("BACKGROUND_ON").at(0) == 1.0){
	//	DrawStarField();
	//}
	
	if(g_pList.GetVectorData("FIX_DISP_EYE").at(0) == 1 || g_pList.GetVectorData("FIX_DISP_EYE").at(0) == 3)
	{
		glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
		// currently, use same color to dots color. we can change it later.
		//glColor3d(g_pList.GetVectorData("STAR_LEYE_COLOR").at(0),g_pList.GetVectorData("STAR_LEYE_COLOR").at(1),g_pList.GetVectorData("STAR_LEYE_COLOR").at(2));
		// changed from above to below by hrk. 2/9/2012
		glColor3d(g_pList.GetVectorData("TARG_RLUM").at(0),g_pList.GetVectorData("TARG_GLUM").at(0),g_pList.GetVectorData("TARG_BLUM").at(0));
		DrawFixation();
	}

	// 05/14/09 hkim add discrimination task target display
	// Target 1
	if (m_starfield.drawTarget1 == 1.0) {
		glPushMatrix();
			glTranslatef(m_starfield.targ1Location[0] + m_Lateral,
				   m_starfield.targ1Location[1] - m_Heave,
				   m_starfield.targ1Location[2] - m_Surge);
			DrawTargetObject(DRAW_TARG1);
		glPopMatrix();
	}

	// Target 2
	if (m_starfield.drawTarget2 == 1.0) {
		/*glColor3d(0.0*m_starfield.targLumMult[2], 1.0*m_starfield.targLumMult[2], 0.0*m_starfield.targLumMult[2]);
		glBegin(GL_POINTS);
		glVertex3d(m_starfield.targ2Location[0] + m_Lateral,
				   m_starfield.targ2Location[1] - m_Heave,
				   m_starfield.targ2Location[2] - m_Surge);
		glEnd();*/
		glPushMatrix();
			glTranslatef(m_starfield.targ2Location[0] + m_Lateral,
				   m_starfield.targ2Location[1] - m_Heave,
				   m_starfield.targ2Location[2] - m_Surge);
			DrawTargetObject(DRAW_TARG2);
		glPopMatrix();
	}

	// Draw cut off mask with FP color
	if (m_starfield.useCutout == true) DrawCutoff();

	if(g_pList.GetVectorData("BINOC_DISP").at(0) < 3.0){  // Left eye should be drawn for all cases except BINOC_DISP == 3
	// if(g_pList.GetVectorData("BINOC_DISP").at(0) == 1.0 || (g_pList.GetVectorData("BINOC_DISP").at(0) == 2.0 && g_pList.GetVectorData("PATCH_DISP_EYE").at(0) == 1.0)){  
		glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
		glColor3d(g_pList.GetVectorData("STAR_LEYE_COLOR").at(0),g_pList.GetVectorData("STAR_LEYE_COLOR").at(1),g_pList.GetVectorData("STAR_LEYE_COLOR").at(2));

		if((int)g_pList.GetVectorData("CTRL_STIM_ON").at(0) == 1)
		{
			 if(m_stimOn == 1) DrawDotFields();
		}
		else DrawDotFields();
	}

	// Draw the left starfield.
	glColor3d(m_starfield.starLeftColor[0] * m_starfield.luminance,		// Red
			m_starfield.starLeftColor[1] * m_starfield.luminance,		// Green
			m_starfield.starLeftColor[2] * m_starfield.luminance);	// Blue
	if (g_pList.GetVectorData("BACKGROUND_ON").at(0) == 1.0){
		DrawStarField();
	}

	//**** BACK RIGHT BUFFER. ****//
#if USE_STEREO
	glDrawBuffer(GL_BACK_RIGHT);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);		// Clears the current scene.
#else
	glColorMask(GL_FALSE, GL_TRUE, GL_FALSE, GL_FALSE);
#endif

	//CalculateStereoFrustum(screen_width, screen_height, view_dist, clip_near, clip_far, eye_separation, 0, 0);
	CalculateStereoFrustum(m_frustum.screenWidth, m_frustum.screenHeight, m_frustum.camera2screenDist,
						   m_frustum.clipNear, m_frustum.clipFar, eye_separation,
						   m_frustum.worldOffsetX, m_frustum.worldOffsetZ);
	glMatrixMode(GL_MODELVIEW);

	glLoadIdentity();

	// Setup the camera.
	switch((int)g_pList.GetVectorData("GLU_LOOK_AT").at(0)){
		case 0:
			gluLookAt(eye_separation+m_Lateral, 0.0f-m_Heave, view_dist-m_Surge,		// Camera origin
						eye_separation+m_Lateral, 0.0f-m_Heave, view_dist-m_Surge-1.0f,	// Camera direction
						0.0, 1.0, 0.0);
			break;
		case 1:
			gluLookAt(m_Lateral, 0.0f-m_Heave, view_dist-m_Surge,		// Camera origin
					    m_fpx, m_fpy, 0.0, 	// Camera direction
					    0.0, 1.0, 0.0);
			glTranslated(-eye_separation,0.0,0.0);  // RM condition is cyclopean now; eye_separation should be zero.
			break;
		case 2:		// Not in use!  Only works for pref_direction = 0.
			r = view_dist/2.0;
			gluLookAt(sin(m_Lateral)*r, 0.0f-m_Heave, -cos(m_Lateral)*-r+r-m_Surge,		// Camera origin
					    m_fpx, m_fpy, 0.0, 	// Camera direction
					    0.0, 1.0, 0.0);
			glTranslated(-eye_separation,0.0,0.0);
			break;
		case 3:
			gluLookAt(eye_separation, 0.0, view_dist-m_Surge,			// Position straight ahead, don't move
						eye_separation, 0.0, view_dist-m_Surge-1.0f,
						0.0, 1.0, 0.0);
	}

	//if(g_pList.GetVectorData("BINOC_DISP").at(0)<2.0 || g_pList.GetVectorData("BINOC_DISP").at(0)==3.0)
	//// if(g_pList.GetVectorData("BINOC_DISP").at(0) == 1.0 || (g_pList.GetVectorData("BINOC_DISP").at(0) == 2.0 && g_pList.GetVectorData("PATCH_DISP_EYE").at(0) == 2.0)){  
	//{  // Right eye should be drawn for all cases except BINOC_DISP == 2
	//	glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
	//	glColor3d(g_pList.GetVectorData("STAR_REYE_COLOR").at(0),g_pList.GetVectorData("STAR_REYE_COLOR").at(1),g_pList.GetVectorData("STAR_REYE_COLOR").at(2));
	//	//DrawDotFields();
	//	//DrawIncongDotFields();
	//	if((int)g_pList.GetVectorData("CTRL_STIM_ON").at(0) == 1)
	//	{
	//		 if(m_stimOn == 1) DrawIncongDotFields();
	//	}
	//	else DrawIncongDotFields();
	//}

	//// Draw the right starfield.
	//glColor3d(m_starfield.starRightColor[0] * m_starfield.luminance,		// Red
	//		m_starfield.starRightColor[1] * m_starfield.luminance,		// Green
	//		m_starfield.starRightColor[2] * m_starfield.luminance);	// Blue
	//if (g_pList.GetVectorData("BACKGROUND_ON").at(0) == 1.0){
	//	DrawStarField();
	//}

	if (g_pList.GetVectorData("FIX_DISP_EYE").at(0) == 2.0 || g_pList.GetVectorData("FIX_DISP_EYE").at(0) == 3.0 )
	{
		glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
		//glColor3d(g_pList.GetVectorData("STAR_REYE_COLOR").at(0),g_pList.GetVectorData("STAR_REYE_COLOR").at(1),g_pList.GetVectorData("STAR_REYE_COLOR").at(2));
		// changed from above to below by hrk. 2/9/2012
		glColor3d(g_pList.GetVectorData("TARG_RLUM").at(0),g_pList.GetVectorData("TARG_GLUM").at(0),g_pList.GetVectorData("TARG_BLUM").at(0));
		DrawFixation();
	}

	// 05/14/09 hkim add discrimination task target display
	// Target 1
	if (m_starfield.drawTarget1 == 1.0) {
		glPushMatrix();
			glTranslatef(m_starfield.targ1Location[0] + m_Lateral,
				   m_starfield.targ1Location[1] - m_Heave,
				   m_starfield.targ1Location[2] - m_Surge);
			DrawTargetObject(DRAW_TARG1);
		glPopMatrix();
	}

	// Target 2
	if (m_starfield.drawTarget2 == 1.0) {
		/*glColor3d(0.0*m_starfield.targLumMult[2], 1.0*m_starfield.targLumMult[2], 0.0*m_starfield.targLumMult[2]);
		glBegin(GL_POINTS);
		glVertex3d(m_starfield.targ2Location[0] + m_Lateral,
				   m_starfield.targ2Location[1] - m_Heave,
				   m_starfield.targ2Location[2] - m_Surge);
		glEnd();*/
		glPushMatrix();
			glTranslatef(m_starfield.targ2Location[0] + m_Lateral,
				   m_starfield.targ2Location[1] - m_Heave,
				   m_starfield.targ2Location[2] - m_Surge);
			DrawTargetObject(DRAW_TARG2);
		glPopMatrix();
	}

	// Draw cut off mask with FP color
	if (m_starfield.useCutout == true) DrawCutoff();

	if(g_pList.GetVectorData("BINOC_DISP").at(0)<2.0 || g_pList.GetVectorData("BINOC_DISP").at(0)==3.0)
	// if(g_pList.GetVectorData("BINOC_DISP").at(0) == 1.0 || (g_pList.GetVectorData("BINOC_DISP").at(0) == 2.0 && g_pList.GetVectorData("PATCH_DISP_EYE").at(0) == 2.0)){  
	{  // Right eye should be drawn for all cases except BINOC_DISP == 2
		glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
		glColor3d(g_pList.GetVectorData("STAR_REYE_COLOR").at(0),g_pList.GetVectorData("STAR_REYE_COLOR").at(1),g_pList.GetVectorData("STAR_REYE_COLOR").at(2));
		//DrawDotFields();
		//DrawIncongDotFields();
		if((int)g_pList.GetVectorData("CTRL_STIM_ON").at(0) == 1)
		{
			 if(m_stimOn == 1) DrawIncongDotFields();
		}
		else DrawIncongDotFields();
	}

	// Draw the right starfield.
	glColor3d(m_starfield.starRightColor[0] * m_starfield.luminance,		// Red
			m_starfield.starRightColor[1] * m_starfield.luminance,		// Green
			m_starfield.starRightColor[2] * m_starfield.luminance);	// Blue
	if (g_pList.GetVectorData("BACKGROUND_ON").at(0) == 1.0){
		DrawStarField();
	}

#if RECORD_MOVIE
		glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
#endif

	glFlush();
}

GLvoid GLPanel::Render()
{
	// RENDER_TYPE=0: this routine. RENDER_TYPE=1: Motion Parallax routine
	if((int)g_pList.GetVectorData("RENDER_TYPE").at(0) == 1)
	{
		RenderMotionParallax();
		return;
	}

	double targOffset = 0.0;

	// If star lifetime is up and we flagged the use of star lifetime, then modify some of
	// the stars.
	//if (m_frameCount % m_starfield.lifetime == 0 && m_starfield.drawBackground == 1.0 && m_starfield.use_lifetime == 1.0) {
	if (m_frameCount % m_starfield.lifetime == 0 && g_pList.GetVectorData("BACKGROUND_ON")[0] == 1.0 && m_starfield.use_lifetime == 1.0) {
		ModifyStarField();
	}
	ModifyAllObjectStarField();

	if(redrawObjTexture)
	{
		GenerateAllObjectStarFieldTexture();
		redrawObjTexture = false;
	}

	m_frameCount++;

	////**** BACK LEFT BUFFER. ****//
#if USE_STEREO
	glDrawBuffer(GL_BACK_LEFT);
#else
	glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
#endif
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);		// Clears the current scene.

	// Setup the projection matrix.
	CalculateStereoFrustum(m_frustum.screenWidth, m_frustum.screenHeight, m_frustum.camera2screenDist,
						   m_frustum.clipNear, m_frustum.clipFar, -m_frustum.eyeSeparation / 2.0f,
						   m_frustum.worldOffsetX, m_frustum.worldOffsetZ);

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	//Simulated(3) or fake(4) pursue case: FP and cutoff mask will be at center and eye mask will not move.
	//m_rotationType == 5 or 6 is the case of FP_ROTATE=0 with mask.
	if(m_rotationType == 3 || m_rotationType == 4 || m_rotationType == 5 || m_rotationType == 6) 
	{	// Draw everything before rotate camera
		glPushMatrix();
			// Setup the camera.
			gluLookAt(-m_frustum.eyeSeparation/2.0f+m_Lateral, 0.0f-m_Heave, m_frustum.camera2screenDist-m_Surge,		// Camera origin
				-m_frustum.eyeSeparation/2.0f+m_Lateral, 0.0f-m_Heave, m_frustum.camera2screenDist-m_Surge-1.0f,	// Camera direction
				0.0, 1.0, 0.0); // Which way is up
			// Draw FP
			if (m_starfield.drawFixationPoint == 1.0) {
				glPushMatrix();
					glTranslatef(m_starfield.fixationPointLocation[0] + m_Lateral,
						m_starfield.fixationPointLocation[1] - m_Heave,
						m_starfield.fixationPointLocation[2] - m_Surge);
					DrawTargetObject(DRAW_FP);
				glPopMatrix();
			}
			// Draw cut off mask with FP color
			if (m_starfield.useCutout == true) DrawCutoff();
			// Draw eye mask
			if(g_pList.GetVectorData("FP_ROTATE_MASK").at(0) > 0.0) 
			{
				glColor3d(0.0, 0.0, 0.0);
				glPushMatrix();
					glTranslated(m_Lateral,-m_Heave,-m_Surge);
					DrawEyeMask(LEFT_EYE);
				glPopMatrix();
			}
		glPopMatrix();

		//Rotate Camera
		if(m_rotationType == 3) glLoadMatrixd(m_rotCameraCoordMatrix[LEFT_EYE].m);
	}

	// Setup the camera.
	gluLookAt(-m_frustum.eyeSeparation/2.0f+m_Lateral, 0.0f-m_Heave, m_frustum.camera2screenDist-m_Surge,		// Camera origin
			  -m_frustum.eyeSeparation/2.0f+m_Lateral, 0.0f-m_Heave, m_frustum.camera2screenDist-m_Surge-1.0f,	// Camera direction
			  0.0, 1.0, 0.0); // Which way is up

	currCameraTrans.set(m_Lateral, 0.0f-m_Heave, -m_Surge);

	glDisable(GL_STENCIL_TEST);

	if(m_rotationType == 2) //pursue case: eye mask moving with FP
	{	// Draw eye mask
		if(g_pList.GetVectorData("FP_ROTATE_MASK").at(0) > 0.0) 
		{
			glColor3d(0.0, 0.0, 0.0);
			glPushMatrix();
				glTranslated(m_Lateral,-m_Heave,-m_Surge);
				DrawEyeMask(LEFT_EYE);
			glPopMatrix();
			glColor3d(1.0, 0.0, 0.0);
		}
	}

	// Create the center dot.  This is not the same as the fixation target.
	// This is mainly used to calibrate movement.
	//glTranslated(calibTranOn[2],calibTranOn[3],calibTranOn[4]);
	//glLineWidth(lineWidth);
	//glColor3d(1.0, 0.0, 0.0);
	//glBegin(GL_LINES);
	//// horizontal line
	//if(calibTranOn[0]== 1){
	//	glVertex3d(lineLength/2, 0.0, 0.0);
	//	glVertex3d(-lineLength/2, 0.0, 0.0);
	//}
	//// vertical line
	//if(calibTranOn[1]== 1){
	//	glVertex3d(0.0, lineLength/2, 0.0);
	//	glVertex3d(0.0, -lineLength/2, 0.0);
	//}
	//glEnd();
	//glTranslated(-calibTranOn[2],-calibTranOn[3],-calibTranOn[4]);

	// Drawing a square for calibration
	if(calibSquareOn == 1){
		glTranslated(squareCenter[0],squareCenter[1],0.0);
		glBegin(GL_LINE_LOOP);
			glVertex3d(squareSize/2,squareSize/2,0.0);
			glVertex3d(-squareSize/2,squareSize/2,0.0);
			glVertex3d(-squareSize/2,-squareSize/2,0.0);
			glVertex3d(squareSize/2,-squareSize/2,0.0);
		glEnd();
		glTranslated(-squareCenter[0],-squareCenter[1],0.0);
	}


	// If we don't want the fixation point rotated, go ahead and draw it at
	// a fixed position in front of the camera.
	if (m_rotationType == 0) {
		//glDisable(GL_STENCIL_TEST);
		glPointSize(FP_DOTSIZE);
		// Fixation point.
		if (m_starfield.drawFixationPoint == 1.0) {
			/* Originally, we draw FP by fixed 5 pixel.
			glColor3d(1.0*m_starfield.targLumMult[0], 0.0*m_starfield.targLumMult[0], 0.0*m_starfield.targLumMult[0]);
			glBegin(GL_POINTS);
			glVertex3d(m_starfield.fixationPointLocation[0] + m_Lateral,
					m_starfield.fixationPointLocation[1] - m_Heave,
					m_starfield.fixationPointLocation[2] - m_Surge);
			glEnd();*/
			glPushMatrix();
				if(g_pList.GetVectorData("FP_TRANSLATION").at(0)==0) // FP moving with camera
					glTranslatef(m_starfield.fixationPointLocation[0] + m_Lateral,
						m_starfield.fixationPointLocation[1] - m_Heave,
						m_starfield.fixationPointLocation[2] - m_Surge);
				else if(g_pList.GetVectorData("FP_TRANSLATION").at(0)==1) // FP stationary
					glTranslatef(m_starfield.fixationPointLocation[0],
						m_starfield.fixationPointLocation[1],
						m_starfield.fixationPointLocation[2]);
				DrawTargetObject(DRAW_FP);
			glPopMatrix();
		}
		//glEnable(GL_STENCIL_TEST);
	}

	// Target 1
	if (m_starfield.drawTarget1 == 1.0) {
		/*glColor3d(1.0*m_starfield.targLumMult[1], 0.0*m_starfield.targLumMult[1], 0.0*m_starfield.targLumMult[1]);
		glBegin(GL_POINTS);
		glVertex3d(m_starfield.targ1Location[0] + m_Lateral,
				   m_starfield.targ1Location[1] - m_Heave,
				   m_starfield.targ1Location[2] - m_Surge);
		glEnd();*/
		glPushMatrix();
			glTranslatef(m_starfield.targ1Location[0] + m_Lateral,
				   m_starfield.targ1Location[1] - m_Heave,
				   m_starfield.targ1Location[2] - m_Surge);
			DrawTargetObject(DRAW_TARG1);
		glPopMatrix();
	}

	// Target 2
	if (m_starfield.drawTarget2 == 1.0) {
		/*glColor3d(1.0*m_starfield.targLumMult[2], 0.0*m_starfield.targLumMult[2], 0.0*m_starfield.targLumMult[2]);
		glBegin(GL_POINTS);
		glVertex3d(m_starfield.targ2Location[0] + m_Lateral,
				   m_starfield.targ2Location[1] - m_Heave,
				   m_starfield.targ2Location[2] - m_Surge);
		glEnd();*/
		glPushMatrix();
			glTranslatef(m_starfield.targ2Location[0] + m_Lateral,
				   m_starfield.targ2Location[1] - m_Heave,
				   m_starfield.targ2Location[2] - m_Surge);
			DrawTargetObject(DRAW_TARG2);
		glPopMatrix();
	}

	DrawObjectTargets(Point3(m_Lateral, -m_Heave, -m_Surge));


	// This is mainly used to calibrate the amplitude of sinusoid rotation movement
	// Drawing fixed max and min line of rotation amplitude.
	if(calibRotOn[0] == 1){
		glLineWidth(lineWidth);
		if (m_doRotation == true){	
			//glPushMatrix();
			glTranslated(m_centerX, m_centerY, m_centerZ);
			glRotated(minPredictedAngle, m_rotationVector.x, m_rotationVector.y, m_rotationVector.z);
			glTranslated(-m_centerX, -m_centerY, -m_centerZ);
			glColor3d(1.0, 1.0, 1.0);
			glBegin(GL_LINES);
			if(calibRotMotion == 0){ // horizontal line (pitch)
				glVertex3d(lineLength/2, 0.0, 0.0);
				glVertex3d(-lineLength/2, 0.0, 0.0);
			}
			else{// vertical line (yaw and roll)
				glVertex3d(0.0, lineLength/2, 0.0);
				glVertex3d(0.0, -lineLength/2, 0.0);
			}
			glEnd();
			//glPopMatrix();

			//glPushMatrix();
			glTranslated(m_centerX, m_centerY, m_centerZ);
			glRotated(maxPredictedAngle-minPredictedAngle, m_rotationVector.x, m_rotationVector.y, m_rotationVector.z);
			glTranslated(-m_centerX, -m_centerY, -m_centerZ);
			glBegin(GL_LINES);
			if(calibRotMotion == 0){ // horizontal line (pitch)
				glVertex3d(lineLength/2, 0.0, 0.0);
				glVertex3d(-lineLength/2, 0.0, 0.0);
			}
			else{// vertical line (yaw and roll)			
				glVertex3d(0.0, lineLength/2, 0.0);
				glVertex3d(0.0, -lineLength/2, 0.0);
			}
			glEnd();
			//glPopMatrix();

			glTranslated(m_centerX, m_centerY, m_centerZ);
			glRotated(-maxPredictedAngle, m_rotationVector.x, m_rotationVector.y, m_rotationVector.z);
			glTranslated(-m_centerX, -m_centerY, -m_centerZ);
		}
	}

	glEnable(GL_STENCIL_TEST);

	// If we are using the cutout, we need to setup the stencil buffer.
	// We only want to cut off the background, so we can draw calibration stuff and FP before cut off mask.
	if (m_rotationType == 0) {
		if (m_starfield.useCutout == true) {
			DrawCutoff();
		}
		else {
			glDisable(GL_STENCIL_TEST);
		}
	}

	// If we're flagged to do so, rotate the the star field.
	if (m_doRotation == true) {
		glPushMatrix();
		glTranslated(m_centerX, m_centerY, m_centerZ);
		glRotated(m_rotationAngle, m_rotationVector.x, m_rotationVector.y, m_rotationVector.z);
		glTranslated(-m_centerX, -m_centerY, -m_centerZ);
	}

	
	// This is mainly used to calibrate the amplitude of sinusoid rotation movement
	// Drawing movement line
	if(calibRotOn[1] == 1){
		glLineWidth(lineWidth);
		if (m_doRotation == true){	
			glColor3d(1.0, 1.0, 1.0);
			glBegin(GL_LINES);
			if(calibRotMotion == 0){ // horizontal line (pitch)
				glVertex3d(lineLength/2, 0.0, 0.0);
				glVertex3d(-lineLength/2, 0.0, 0.0);
			}
			else{// vertical line (yaw and roll)
				glVertex3d(0.0, lineLength/2, 0.0);
				glVertex3d(0.0, -lineLength/2, 0.0);
			}
			glEnd();
		}
	}
	

	// Rotate the fixation point.  It will only be rotated if we're flagged to do a
	// rotation transformation.  Otherwise, it's just like the standard fixation point.
	if (m_rotationType == 1 || m_rotationType == 2) {
		glPointSize(FP_DOTSIZE);
		// Fixation point.
		if (m_starfield.drawFixationPoint == 1.0) {
			/* Originally, we draw FP by fixed 5 pixel.
			glColor3d(1.0, 0.0, 0.0);
			glBegin(GL_POINTS);
			glVertex3d(m_starfield.fixationPointLocation[0] + m_Lateral,
					   m_starfield.fixationPointLocation[1] - m_Heave,
					   m_starfield.fixationPointLocation[2] - m_Surge);
			glEnd();*/
			//glDisable(GL_STENCIL_TEST);
			glPushMatrix();
				glTranslatef(m_starfield.fixationPointLocation[0] + m_Lateral,
					m_starfield.fixationPointLocation[1] - m_Heave,
					m_starfield.fixationPointLocation[2] - m_Surge);
				DrawTargetObject(DRAW_FP);
			glPopMatrix();
			//glEnable(GL_STENCIL_TEST);
		}

		// If we are using the cutout, we need to setup the stencil buffer.
		// We only want to cut off the background, so we can draw calibration stuff and FP before cut off mask.
		// Also the cut off mask always move with FP. When FP rotate, cut off mask rotate too.
		if (m_starfield.useCutout == true) {
			DrawCutoff();
		}
		else {
			glDisable(GL_STENCIL_TEST);
		}
	}

	// If we're doing rotation, but we don't want to rotate the background,
	// pop off the rotated modelview matrix to get back to the normal one.
	if (m_rotationType == 2) {
		glPopMatrix();
	}

	if(g_pList.GetVectorData("BINOC_DISP").at(0)!=BINOC_DISP_RIGHT_EYE)
	{
		//Setup the stentil buffer and only drawing all stimulus inside the mask.
		if(stimMaskVars.at(2) > 0.0) DrawStimMask();

		// Draw the left starfield.
		glColor3d(m_starfield.starLeftColor[0] * m_starfield.luminance,		// Red
				m_starfield.starLeftColor[1] * m_starfield.luminance,		// Green
				m_starfield.starLeftColor[2] * m_starfield.luminance);	// Blue
		//if (m_starfield.drawBackground == 1.0) {
		if (g_pList.GetVectorData("BACKGROUND_ON").at(0) == 1.0){
			DrawStarField();
		}

		// Draw the left starfield.
		RenderAllObj(LEFT_EYE);

		// Draw world reference object
		DrawWorldRefObj(LEFT_EYE);
	}
	// Draw a cross in OpenGL coord system for calibrating movement.
	DrawCross(calibTranOn[2],calibTranOn[3],calibTranOn[4], lineLength, lineWidth, LEFT_EYE);

	// Johnny add - 10/21/07
	if (m_doRotation == true) {
			glPopMatrix();
	}

	// Draw FP cross at Screen or at Edge of sphere
	if(FP_cross[0] == 1.0){ // FP cross On
		if(FP_cross[1] == 1.0) glTranslated(0.0, 0.0, m_frustum.camera2screenDist-hollowSphereRadius);
		if(FP_cross[4] == 1.0){ // add shadow cross
			glLineWidth(FP_cross[5]);
			glColor3d(0.0, 0.0, 0.0);
			glBegin(GL_LINES);
				// horizontal line
				glVertex3d(FP_cross[2]/2, 0.0, 0.0);
				glVertex3d(-FP_cross[2]/2, 0.0, 0.0);
				// vertical line
				glVertex3d(0.0, FP_cross[2]/2, 0.0);
				glVertex3d(0.0, -FP_cross[2]/2, 0.0);
			glEnd();
		}

		// draw the cross
		glLineWidth(FP_cross[3]);
		glColor3d(1.0, 0.0, 0.0);
		glBegin(GL_LINES);
			// horizontal line
			glVertex3d(FP_cross[2]/2, 0.0, 0.0);
			glVertex3d(-FP_cross[2]/2, 0.0, 0.0);
			// vertical line
			glVertex3d(0.0, FP_cross[2]/2, 0.0);
			glVertex3d(0.0, -FP_cross[2]/2, 0.0);
		glEnd();
		if(FP_cross[1] == 1.0) glTranslated(0.0, 0.0, -m_frustum.camera2screenDist+hollowSphereRadius);
	}

	//**** BACK RIGHT BUFFER. ****//
#if USE_STEREO
	glDrawBuffer(GL_BACK_RIGHT);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);		// Clears the current scene.
#else
	glColorMask(GL_FALSE, GL_TRUE, GL_FALSE, GL_FALSE);
#endif

	// Setup the projection matrix.
	CalculateStereoFrustum(m_frustum.screenWidth, m_frustum.screenHeight, m_frustum.camera2screenDist,
						   m_frustum.clipNear, m_frustum.clipFar, m_frustum.eyeSeparation / 2.0f,
						   m_frustum.worldOffsetX, m_frustum.worldOffsetZ);

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	if(m_rotationType == 3 || m_rotationType == 4 || m_rotationType == 5 || m_rotationType == 6) 
	{	// Draw FP at center
		glPushMatrix();
			// Setup the camera.
			gluLookAt(m_frustum.eyeSeparation/2.0f+m_Lateral, 0.0f-m_Heave, m_frustum.camera2screenDist-m_Surge,		// Camera origin
				m_frustum.eyeSeparation/2.0f+m_Lateral, 0.0f-m_Heave, m_frustum.camera2screenDist-m_Surge-1.0f,	// Camera direction
				0.0, 1.0, 0.0);																					// Which way is up
			// Draw FP
			if (m_starfield.drawFixationPoint == 1.0) {
				glPushMatrix();
					glTranslatef(m_starfield.fixationPointLocation[0] + m_Lateral,
						m_starfield.fixationPointLocation[1] - m_Heave,
						m_starfield.fixationPointLocation[2] - m_Surge);
					DrawTargetObject(DRAW_FP);
				glPopMatrix();
			}
			// use cut off mask with FP color
			if (m_starfield.useCutout == true) DrawCutoff();
			// Draw eye mask
			if(g_pList.GetVectorData("FP_ROTATE_MASK").at(0) > 0.0) 
			{
				glColor3d(0.0, 0.0, 0.0);
				glPushMatrix();
					glTranslated(m_Lateral,-m_Heave,-m_Surge);
					DrawEyeMask(RIGHT_EYE);
				glPopMatrix();
			}
		glPopMatrix();

		// Rotate camera coord
		if(m_rotationType == 3) glLoadMatrixd(m_rotCameraCoordMatrix[RIGHT_EYE].m);
	}
	// Setup the camera.
	gluLookAt(m_frustum.eyeSeparation/2.0f+m_Lateral, 0.0f-m_Heave, m_frustum.camera2screenDist-m_Surge,		// Camera origin
			  m_frustum.eyeSeparation/2.0f+m_Lateral, 0.0f-m_Heave, m_frustum.camera2screenDist-m_Surge-1.0f,	// Camera direction
			  0.0, 1.0, 0.0);																					// Which way is up

	glDisable(GL_STENCIL_TEST);

	if(m_rotationType == 2) 
	{	// Draw eye mask
		if(g_pList.GetVectorData("FP_ROTATE_MASK").at(0) > 0.0) 
		{
			glColor3d(0.0, 0.0, 0.0);
			glPushMatrix();
				glTranslated(m_Lateral,-m_Heave,-m_Surge);
				DrawEyeMask(RIGHT_EYE);
			glPopMatrix();		
			glColor3d(0.0, 1.0, 0.0);
		}
	}

	// Create the center dot.  This is not the same as the fixation target.
	// This is mainly used to calibrate movement.
	//glTranslated(calibTranOn[2],calibTranOn[3],calibTranOn[4]);
	//glLineWidth(lineWidth);
	//glColor3d(0.0, 1.0, 0.0);
	//glBegin(GL_LINES);
	//// horizontal line
	//if(calibTranOn[0]== 1){
	//	glVertex3d(lineLength/2, 0.0, 0.0);
	//	glVertex3d(-lineLength/2, 0.0, 0.0);
	//}
	//// vertical line
	//if(calibTranOn[1]== 1){
	//	glVertex3d(0.0, lineLength/2, 0.0);
	//	glVertex3d(0.0, -lineLength/2, 0.0);
	//}
	//glEnd();
	//glTranslated(-calibTranOn[2],-calibTranOn[3],-calibTranOn[4]);

	// If we don't want the fixation point rotated, go ahead and draw it at
	// a fixed position in front of the camera.
	if (m_rotationType == 0) {
		//glDisable(GL_STENCIL_TEST);
		glPointSize(FP_DOTSIZE);
		// Fixation point.
		if (m_starfield.drawFixationPoint == 1.0) {
			/* Originally, we draw FP by fixed 5 pixel.
			glColor3d(0.0*m_starfield.targLumMult[0], 1.0*m_starfield.targLumMult[0], 0.0*m_starfield.targLumMult[0]);
			glBegin(GL_POINTS);
			glVertex3d(m_starfield.fixationPointLocation[0] + m_Lateral,
					m_starfield.fixationPointLocation[1] - m_Heave,
					m_starfield.fixationPointLocation[2] - m_Surge);
			glEnd();*/
			glPushMatrix();
				if(g_pList.GetVectorData("FP_TRANSLATION").at(0)==0) // FP moving with camera
					glTranslatef(m_starfield.fixationPointLocation[0] + m_Lateral,
						m_starfield.fixationPointLocation[1] - m_Heave,
						m_starfield.fixationPointLocation[2] - m_Surge);
				else if(g_pList.GetVectorData("FP_TRANSLATION").at(0)==1) // FP stationary
					glTranslatef(m_starfield.fixationPointLocation[0],
						m_starfield.fixationPointLocation[1],
						m_starfield.fixationPointLocation[2]);
				DrawTargetObject(DRAW_FP);
			glPopMatrix();
		}
		//glEnable(GL_STENCIL_TEST);
	}

	// Target 1
	if (m_starfield.drawTarget1 == 1.0) {
		/*glColor3d(0.0*m_starfield.targLumMult[1], 1.0*m_starfield.targLumMult[1], 0.0*m_starfield.targLumMult[1]);
		glBegin(GL_POINTS);
		glVertex3d(m_starfield.targ1Location[0] + m_Lateral,
				   m_starfield.targ1Location[1] - m_Heave,
				   m_starfield.targ1Location[2] - m_Surge);
		glEnd();*/
		glPushMatrix();
			glTranslatef(m_starfield.targ1Location[0] + m_Lateral,
				   m_starfield.targ1Location[1] - m_Heave,
				   m_starfield.targ1Location[2] - m_Surge);
			DrawTargetObject(DRAW_TARG1);
		glPopMatrix();
	}

	// Target 2
	if (m_starfield.drawTarget2 == 1.0) {
		/*glColor3d(0.0*m_starfield.targLumMult[2], 1.0*m_starfield.targLumMult[2], 0.0*m_starfield.targLumMult[2]);
		glBegin(GL_POINTS);
		glVertex3d(m_starfield.targ2Location[0] + m_Lateral,
				   m_starfield.targ2Location[1] - m_Heave,
				   m_starfield.targ2Location[2] - m_Surge);
		glEnd();*/
		glPushMatrix();
			glTranslatef(m_starfield.targ2Location[0] + m_Lateral,
				   m_starfield.targ2Location[1] - m_Heave,
				   m_starfield.targ2Location[2] - m_Surge);
			DrawTargetObject(DRAW_TARG2);
		glPopMatrix();
	}

	DrawObjectTargets(Point3(m_Lateral, -m_Heave, -m_Surge));

	glEnable(GL_STENCIL_TEST);

#if USE_STEREO
	// If we are using the cutout, we need to setup the stencil buffer.
	// We only want to cut off the background, so we can draw calibration stuff and FP before cut off mask.
	if (m_rotationType == 0) {
		if (m_starfield.useCutout == true) {
			DrawCutoff();
		}
		else {
			glDisable(GL_STENCIL_TEST);
		}
	}
#endif

	// If we're flagged to do so, rotate the the star field.
	if (m_doRotation == true) {
		glPushMatrix();
		glTranslated(m_centerX, m_centerY, m_centerZ);
		glRotated(m_rotationAngle, m_rotationVector.x, m_rotationVector.y, m_rotationVector.z);
		glTranslated(-m_centerX, -m_centerY, -m_centerZ);
	}

	// Rotate the fixation point.  It will only be rotated if we're flagged to do a
	// rotation transformation.  Otherwise, it's just like the standard fixation point.
	if (m_rotationType == 1 || m_rotationType == 2) {
		glPointSize(FP_DOTSIZE);
		// Fixation point.
		if (m_starfield.drawFixationPoint == 1.0) {
			/* Originally, we draw FP by fixed 5 pixel.
			glColor3d(0.0, 1.0, 0.0);
			glBegin(GL_POINTS);
			glVertex3d(m_starfield.fixationPointLocation[0] + m_Lateral,
				m_starfield.fixationPointLocation[1] - m_Heave,
				m_starfield.fixationPointLocation[2] - m_Surge);
			glEnd();*/
			//glDisable(GL_STENCIL_TEST);
			glPushMatrix();
				glTranslatef(m_starfield.fixationPointLocation[0] + m_Lateral,
					m_starfield.fixationPointLocation[1] - m_Heave,
					m_starfield.fixationPointLocation[2] - m_Surge);
				DrawTargetObject(DRAW_FP);
			glPopMatrix();
			//glEnable(GL_STENCIL_TEST);
		}

#if USE_STEREO
		// If we are using the cutout, we need to setup the stencil buffer.
		// We only want to cut off the background, so we can draw calibration stuff and FP before cut off mask.
		// Also the cut off mask always move with FP. When FP rotate, cut off mask rotate too.
		if (m_starfield.useCutout == true) {
			DrawCutoff();
		}
		else {
			glDisable(GL_STENCIL_TEST);
		}
#endif
	}

	// If we're doing rotation, but we don't want to rotate the background,
	// pop off the rotated modelview matrix to get back to the normal one.
	if (m_rotationType == 2) {
		glPopMatrix();
	}

	if(g_pList.GetVectorData("BINOC_DISP").at(0)!=BINOC_DISP_LEFT_EYE)
	{
		//Setup the stentil buffer and only drawing all stimulus inside the mask.
		if(stimMaskVars.at(2) > 0.0) DrawStimMask();

		// Draw the right starfield.
		glColor3d(m_starfield.starRightColor[0] * m_starfield.luminance,		// Red
				m_starfield.starRightColor[1] * m_starfield.luminance,		// Green
				m_starfield.starRightColor[2] * m_starfield.luminance);		// Blue
		//if (m_starfield.drawBackground == 1.0) {
		if (g_pList.GetVectorData("BACKGROUND_ON").at(0) == 1.0){
			DrawStarField();

		}

		// Draw the right object starfield.
		RenderAllObj(RIGHT_EYE);

		// Draw world reference object
		DrawWorldRefObj(RIGHT_EYE);
	}

	// Draw a cross in OpenGL coord system for calibrating movement.
	DrawCross(calibTranOn[2],calibTranOn[3],calibTranOn[4], lineLength, lineWidth, RIGHT_EYE);

	// Johnny add - 10/21/07
	if (m_doRotation == true) {
			glPopMatrix();
	}

		// Draw FP cross at Screen or at Edge of sphere
	if(FP_cross[0] == 1.0){ // FP cross On
		if(FP_cross[1] == 1.0) glTranslated(0.0, 0.0, m_frustum.camera2screenDist-hollowSphereRadius);
		if(FP_cross[4] == 1.0){ // add shadow cross
			glLineWidth(FP_cross[5]);
			glColor3d(0.0, 0.0, 0.0);
			glBegin(GL_LINES);
				// horizontal line
				glVertex3d(FP_cross[2]/2, 0.0, 0.0);
				glVertex3d(-FP_cross[2]/2, 0.0, 0.0);
				// vertical line
				glVertex3d(0.0, FP_cross[2]/2, 0.0);
				glVertex3d(0.0, -FP_cross[2]/2, 0.0);
			glEnd();
		}

		// draw the cross
		glLineWidth(FP_cross[3]);
		glColor3d(0.0, 1.0, 0.0);
		glBegin(GL_LINES);
			// horizontal line
			glVertex3d(FP_cross[2]/2, 0.0, 0.0);
			glVertex3d(-FP_cross[2]/2, 0.0, 0.0);
			// vertical line
			glVertex3d(0.0, FP_cross[2]/2, 0.0);
			glVertex3d(0.0, -FP_cross[2]/2, 0.0);
		glEnd();
		if(FP_cross[1] == 1.0) glTranslated(0.0, 0.0, -m_frustum.camera2screenDist+hollowSphereRadius);
	}

#if RECORD_MOVIE
		glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
#endif

	//glFlush();
	glFinish();
}

GLvoid GLPanel::RenderAllObj(int whichEye)
{
	for(int i=0; i<NUM_OF_DYN_OBJ; i++)
	{		
		if (glDynObject[i]->GetObject().enable == 1.0 && glDynObject[i]->GetObject().density > 0.0)
		{
			glPushMatrix();
			glDynObject[i]->RenderObj(whichEye);
			glPopMatrix();

			if(g_pList.GetVectorData("OBJ_CENTER_CROSS").at(0)==1.0)
			{
				XYZ3 pos = glDynObject[i]->GetObjFieldTran();
				DrawCross(pos.x, pos.y, pos.z, lineLength, lineWidth, whichEye);
			}
		}
	}
	
	if(redrawCallList) SetupStaticObjCallList(whichEye);
	glCallList(m_staticObjCallList[whichEye]);
}

GLvoid GLPanel::SetupStaticObjCallList(int whichEye)
{
	if(glIsList(m_staticObjCallList[whichEye]))
		glDeleteLists(m_staticObjCallList[whichEye],1);

	glNewList(m_staticObjCallList[whichEye], GL_COMPILE);
	XYZ3 objOrigin;
	for(int i=0; i<NUM_OF_STATIC_OBJ; i++)
	{
		if (glStaticObject[i]->GetObject().enable == 1.0 && glStaticObject[i]->GetObject().density > 0.0)
		{	
			glPushMatrix();
			//glStaticObject[i]->SetObjFieldTran(	g_pList.GetVectorData("STATIC_OBJ_M_ORIGIN").at(i*3+0)*100.0,
			//									g_pList.GetVectorData("STATIC_OBJ_M_ORIGIN").at(i*3+1)*100.0,
			//									g_pList.GetVectorData("STATIC_OBJ_M_ORIGIN").at(i*3+2)*100.0);
			objOrigin = glStaticObject[i]->GetObjOrigin();
			glStaticObject[i]->SetObjFieldTran(objOrigin.x*100.0, objOrigin.y*100.0, objOrigin.z*100.0);
			glStaticObject[i]->RenderObj(whichEye);
			glPopMatrix();

			if(g_pList.GetVectorData("OBJ_CENTER_CROSS").at(0)==1.0)
			{
				XYZ3 pos = glStaticObject[i]->GetObjFieldTran();
				DrawCross(pos.x, pos.y, pos.z, lineLength, lineWidth, whichEye);
			}
		}
	}
	glEndList();
}

void GLPanel::DoRotation(bool val)
{
	m_doRotation = val;
}


void GLPanel::SetRotationVector(nm3DDatum rotationVector)
{
	m_rotationVector = rotationVector;
}

void GLPanel::SetRotCameraCoordMatrix(int whichEye, Affine4Matrix rotCameraCoordMatrix)
{
	m_rotCameraCoordMatrix[whichEye].set(rotCameraCoordMatrix);
}

void GLPanel::SetEyeMask(int whichEye, EyeMask eyeMask)
{
	m_eyeMask[whichEye] = eyeMask;
}

void GLPanel::SetRotationAngle(double angle)
{
	m_rotationAngle = angle;
}

void GLPanel::SetRotationCenter(double x, double y, double z)
{
	m_centerX = x;
	m_centerY = y;
	m_centerZ = z;
}


GLvoid GLPanel::DrawStarField()
{	
	/* Chris original code
	int i;

	// Don't try to mess with an unallocated array.
	if (m_starArray == NULL) {
		return;
	}

	for (i = 0; i < m_starfield.totalStars; i++) {
		glBegin(GL_TRIANGLES);
			glVertex3d(m_starArray[i].x[0], m_starArray[i].y[0], m_starArray[i].z[0]);
			glVertex3d(m_starArray[i].x[1], m_starArray[i].y[1], m_starArray[i].z[1]);
			glVertex3d(m_starArray[i].x[2], m_starArray[i].y[2], m_starArray[i].z[2]);
		glEnd();
	}
	*/

	if (g_pList.GetVectorData("DRAW_MODE").at(0) == 1.0){ // triangles' cube
		int j = m_totalStars*3*3;
		glEnableClientState(GL_VERTEX_ARRAY);
		glVertexPointer(3, GL_FLOAT, 0, starFieldVertex3D);
		glDrawArrays(GL_TRIANGLES,0,j/3);
		glDisableClientState(GL_VERTEX_ARRAY);
	}
	else if (g_pList.GetVectorData("DRAW_MODE").at(0) == 0.0){ // Dots' sphere
		glPointSize(g_pList.GetVectorData("DOTS_SIZE").at(0));
		int j = m_totalStars*3;
		glEnableClientState(GL_VERTEX_ARRAY);
		glVertexPointer(3, GL_FLOAT, 0, starFieldVertex3D);
		glDrawArrays(GL_POINTS,0,j/3);
		glDisableClientState(GL_VERTEX_ARRAY);
	}
	else if (g_pList.GetVectorData("DRAW_MODE").at(0) == 2.0){ // Dots' cube
		glPointSize(g_pList.GetVectorData("DOTS_SIZE").at(0));
		int j = m_totalStars*3;
		glEnableClientState(GL_VERTEX_ARRAY);
		glVertexPointer(3, GL_FLOAT, 0, starFieldVertex3D);
		glDrawArrays(GL_POINTS,0,j/3);
		glDisableClientState(GL_VERTEX_ARRAY);
	}
	else if (g_pList.GetVectorData("DRAW_MODE").at(0) == 3.0){ // Dots' hollow shpere
		if(!glIsList(m_starFieldCallList)) SetupCallList();
		if(redrawCallList){
			glDeleteLists(m_starFieldCallList,1);
			SetupCallList();
			redrawCallList = false;
		}
		glCallList(m_starFieldCallList);
	}
}

GLvoid GLPanel::SetupCallList()
{
	int pixelWidth = 1280;	// viewport pixel width
	if(RIG_ROOM == MOOG_164) pixelWidth = 1024*1280/1400;
	//else if(RIG_ROOM == MOOG_157) pixelWidth = 1010;

	double radius = g_pList.GetVectorData("DOTS_SPHERE_PARA").at(0);

	int totalDots = 0;
	double starRadius = 5.0; //cm
	double starInc = 360.0/20.0;

	glNewList(m_starFieldCallList, GL_COMPILE);
	glPushMatrix();
	// translate everything to the center (eye location)
	glTranslatef(m_frustum.worldOffsetX,m_frustum.worldOffsetZ,m_frustum.camera2screenDist);
	for(int i = 0; i<(int)dotSizeDistribute.size(); i++){
		starRadius = ((double)(i+1)/(double)pixelWidth*m_frustum.screenWidth)/2.0;
		for(int j = 0; j < (int)dotSizeDistribute.at(i); j++){
			glPushMatrix();
				glRotatef(aziVector.at(totalDots+j)*RAD2DEG,0,1,0); // rotate at y-axis
				glRotatef(eleVector.at(totalDots+j)*RAD2DEG,0,0,1); // rotate at z-axis
				glTranslatef(radius,0,0);
				glRotatef(90,0,1,0);
				glBegin(GL_TRIANGLE_FAN); // draw circle in starRadius at origin on XY plan
					glVertex3d(0.0, 0.0, 0.0);
					for(double dAngle = 10; dAngle <= 370.0; dAngle += starInc) {
						glVertex3d(starRadius * cos(dAngle*DEG2RAD) + 0.0, //x
								starRadius * sin(dAngle*DEG2RAD) + 0.0, //y
								0.0); //z
					}
				glEnd();
			glPopMatrix();
		}
		totalDots+=dotSizeDistribute.at(i);
	}
	
	glPopMatrix();
	glEndList();

}

GLvoid GLPanel::ModifyStarField()
{
	int i;
	double baseX, baseY, baseZ, prob,
		   sd0, sd1, sd2,
		   ts0, ts1;

	// Grab the starfield dimensions and triangle size.  Pulling this stuff out
	// of the vectors now produces a 20 fold increase in speed in the following
	// loop.
	sd0 = m_starfield.dimensions[0];
	sd1 = m_starfield.dimensions[1];
	sd2 = m_starfield.dimensions[2];
	ts0 = m_starfield.triangle_size[0];
	ts1 = m_starfield.triangle_size[1];

	// don't draw any dots inside the m_frustum.clipNear
	// put all dots between two spheres
	double min = m_frustum.clipNear*m_frustum.clipNear*
		(1+(m_frustum.screenWidth*m_frustum.screenWidth+m_frustum.screenHeight*m_frustum.screenHeight)/
		m_frustum.camera2screenDist/m_frustum.camera2screenDist/4);
	double minDist = sd0;
	if(minDist > sd1) minDist = sd1;
	if(minDist > sd2) minDist = sd2;
	double max = minDist*minDist/4;

	int j = 0;
	for (i = 0; i < m_totalStars; i++) {
		// If a star is in our probability range, we'll modify it.
		prob = (double)rand()/(double)RAND_MAX*100.0;
		//double prob = 100.0;

		// If the coherence factor is higher than a random number chosen between
		// 0 and 100, then we don't do anything to the star.  This means that
		// (100-coherence)% of the total stars will change.
		if (m_starfield.probability < prob) {
			// Find a random point to base our triangle around.
			baseX = (double)rand()/(double)RAND_MAX*sd0 - sd0/2.0;
			baseY = (double)rand()/(double)RAND_MAX*sd1 - sd1/2.0;
			baseZ = (double)rand()/(double)RAND_MAX*sd2 - sd2/2.0;

			// Vertex 1
			m_starArray[i].x[0] = baseX - ts0/2.0;
			m_starArray[i].y[0] = baseY - ts1/2.0;
			m_starArray[i].z[0] = baseZ;

			// Vertex 2
			m_starArray[i].x[1] = baseX;
			m_starArray[i].y[1] = baseY + ts1/2.0;
			m_starArray[i].z[1] = baseZ;

			// Vertex 3
			m_starArray[i].x[2] = baseX + ts0/2.0;
			m_starArray[i].y[2] = baseY - ts1/2.0;
			m_starArray[i].z[2] = baseZ;
		}

		// prepare glarray
		if (m_starfield.drawMode == 1.0){ // triangles
			starFieldVertex3D[j++] = m_starArray[i].x[0]; starFieldVertex3D[j++] = m_starArray[i].y[0]; starFieldVertex3D[j++] = m_starArray[i].z[0];
			starFieldVertex3D[j++] = m_starArray[i].x[1]; starFieldVertex3D[j++] = m_starArray[i].y[1]; starFieldVertex3D[j++] = m_starArray[i].z[1];
			starFieldVertex3D[j++] = m_starArray[i].x[2]; starFieldVertex3D[j++] = m_starArray[i].y[2]; starFieldVertex3D[j++] = m_starArray[i].z[2];
		}
		else if (m_starfield.drawMode == 0.0){ // Dots
			// borrow star field big array
			// don't draw any dots inside the m_frustum.clipNear
			// put all dots between two spheres and shift to the eye center
			if (m_starfield.probability < prob){ // using new location
				if((baseX*baseX + baseY*baseY + baseZ*baseZ > min) && (baseX*baseX + baseY*baseY + baseZ*baseZ < max)){
					starFieldVertex3D[j++] = baseX+m_frustum.worldOffsetX; // Horizontal
					starFieldVertex3D[j++] = baseY+m_frustum.worldOffsetZ; // Vertical
					starFieldVertex3D[j++] = baseZ+m_frustum.camera2screenDist;
				}
				else i--;
			}
			else j = j+3; // keep old location
		}

	}
}

GLvoid GLPanel::ModifyAllObjectStarField()
{
	for(int i=0; i<NUM_OF_DYN_OBJ; i++)
	{
		if (m_frameCount % glDynObject[i]->GetObject().lifetime == 0 && 
			glDynObject[i]->GetObject().enable == 1.0 && 
			glDynObject[i]->GetObject().use_lifetime == 1.0 && 
			glDynObject[i]->GetObject().starDistrubute == 0.0){	// random distrubution
			glDynObject[i]->ModifyObjectStarField();
		}
	}
}

GLvoid GLPanel::GenerateStarField()
{
	int i;
	double baseX, baseY, baseZ,
		   sd0, sd1, sd2,
		   ts0, ts1;
	double radius, density, sigma, mean, azimuth, elevation;
	//vector <double> aziVector, eleVector;

	// Delete the old Star array if needed.
	if (m_starArray != NULL) {
		delete [] m_starArray;
	}

	// Seed the random number generator.
	srand((unsigned)time(NULL));

	// Determine the total number of stars needed to create an average density determined
	// from the StarField structure.
	if (m_starfield.drawMode == 3.0){ // Dots' hollow sphere
		radius = g_pList.GetVectorData("DOTS_SPHERE_PARA").at(0);
		density = g_pList.GetVectorData("DOTS_SPHERE_PARA").at(1);
		m_totalStars = 4*PI*radius*radius*density;
	}
	else
		m_totalStars = (int)(m_starfield.dimensions[0] * m_starfield.dimensions[1] *
					               m_starfield.dimensions[2] * m_starfield.density);

	// Allocate the Star array.
	m_starArray = new Star[m_totalStars];

	// Grab the starfield dimensions and triangle size.  Pulling this stuff out
	// of the vectors now produces a 20 fold increase in speed in the following
	// loop.
	sd0 = m_starfield.dimensions[0];
	sd1 = m_starfield.dimensions[1];
	sd2 = m_starfield.dimensions[2];
	ts0 = m_starfield.triangle_size[0];
	ts1 = m_starfield.triangle_size[1];

	// using glarray to draw star field
	if(starFieldVertex3D != NULL){
		delete [] starFieldVertex3D;
	}
	starFieldVertex3D = new GLfloat[m_totalStars*3*3];

	// don't draw any dots inside the m_frustum.clipNear
	// put all dots between two spheres
	double min = m_frustum.clipNear*m_frustum.clipNear*
		(1+(m_frustum.screenWidth*m_frustum.screenWidth+m_frustum.screenHeight*m_frustum.screenHeight)/
		m_frustum.camera2screenDist/m_frustum.camera2screenDist/4);
	double minDist = sd0;
	if(minDist > sd1) minDist = sd1;
	if(minDist > sd2) minDist = sd2;
	double max = minDist*minDist/4;

	aziVector.clear(); eleVector.clear();
	int j = 0;
    for (i = 0; i < m_totalStars; i++) {

		if (m_starfield.drawMode == 3.0){ // drawing Dots' hollow sphere -- monkey eye at center
			int tmp = j%2;
			// Find a random point on hollow sphere.
			azimuth = ((double)rand()/(double)RAND_MAX)*2*PI;			//0 <= azimth <= 360
			elevation = asin(((double)rand()/(double)RAND_MAX)*2-1);	//-90(-1) <= elevation <= 90(1); The Lambert Projection
			//starFieldVertex3D[j++] = radius*cos(elevation)*cos(azimth) + m_frustum.worldOffsetX; // Horizontal (X)
			//starFieldVertex3D[j++] = radius*sin(elevation) + m_frustum.worldOffsetZ; // Vertical (Y)
			//starFieldVertex3D[j++] = radius*cos(elevation)*sin(azimth) + m_frustum.camera2screenDist; // (Z)
			aziVector.push_back(azimuth);
			eleVector.push_back(elevation);
		}
		else{
			// Find a random point to base our triangle around.
			baseX = (double)rand()/(double)RAND_MAX*sd0 - sd0/2.0;
			baseY = (double)rand()/(double)RAND_MAX*sd1 - sd1/2.0;
			baseZ = (double)rand()/(double)RAND_MAX*sd2 - sd2/2.0;

			// Vertex 1
			m_starArray[i].x[0] = baseX - ts0/2.0;
			m_starArray[i].y[0] = baseY - ts1/2.0;
			m_starArray[i].z[0] = baseZ;

			// Vertex 2
			m_starArray[i].x[1] = baseX;
			m_starArray[i].y[1] = baseY + ts1/2.0;
			m_starArray[i].z[1] = baseZ;

			// Vertex 3
			m_starArray[i].x[2] = baseX + ts0/2.0;
			m_starArray[i].y[2] = baseY - ts1/2.0;
			m_starArray[i].z[2] = baseZ;
		}

		if (m_starfield.drawMode == 1.0){ // Triangles' cube
			// prepare glarray
			starFieldVertex3D[j++] = m_starArray[i].x[0]; starFieldVertex3D[j++] = m_starArray[i].y[0]; starFieldVertex3D[j++] = m_starArray[i].z[0];
			starFieldVertex3D[j++] = m_starArray[i].x[1]; starFieldVertex3D[j++] = m_starArray[i].y[1]; starFieldVertex3D[j++] = m_starArray[i].z[1];
			starFieldVertex3D[j++] = m_starArray[i].x[2]; starFieldVertex3D[j++] = m_starArray[i].y[2]; starFieldVertex3D[j++] = m_starArray[i].z[2];
		}
		else if (m_starfield.drawMode == 0.0){ // Dots' sphere
			// borrow star field big array
			// don't draw any dots inside the m_frustum.clipNear
			// put all dots between two spheres and shift to the eye center
			if((baseX*baseX + baseY*baseY + baseZ*baseZ > min) && (baseX*baseX + baseY*baseY + baseZ*baseZ < max)){
				starFieldVertex3D[j++] = baseX+m_frustum.worldOffsetX; // Horizontal
				starFieldVertex3D[j++] = baseY+m_frustum.worldOffsetZ;//+GL_OFFSET; // Vertical
				starFieldVertex3D[j++] = baseZ+m_frustum.camera2screenDist;
			}
			else i--;
		}
		else if (m_starfield.drawMode == 2.0){ // Dots' cube
				starFieldVertex3D[j++] = baseX; 
				starFieldVertex3D[j++] = baseY;
				starFieldVertex3D[j++] = baseZ;
		}
	}

	// added by HRK. 01/12/2012
	if (g_pList.GetVectorData("CUTOUT_STARFIELD_MODE").at(0) != 0) {
		double pref_direction = g_pList.GetVectorData("PREF_DIRECTION").at(0);
		double eye_shift=0;
		double view_dist = m_frustum.camera2screenDist;
		double dot_depth_signal = g_pList.GetVectorData("PATCH_DEPTH").at(0);
		double dot_bin_corr = g_pList.GetVectorData("PATCH_BIN_CORR").at(0);
		int bin_corr_crit = (int)(0.01* dot_bin_corr * (float)RAND_MAX );
		double dot_depth_min = g_pList.GetVectorData("PATCH_DEPTH_RANGE").at(0);
		double dot_depth_max = g_pList.GetVectorData("PATCH_DEPTH_RANGE").at(1);
		double rf_x = g_pList.GetVectorData("PATCH_LOCATION").at(0);
		double rf_y = g_pList.GetVectorData("PATCH_LOCATION").at(1);
		double rf_dia = g_pList.GetVectorData("PATCH_DIAMETER").at(0);
		CutOutBGDotsByCylinder(starFieldVertex3D, m_totalStars, pref_direction, eye_shift, view_dist,
									dot_depth_signal, bin_corr_crit, dot_depth_min, dot_depth_max,
									rf_x, rf_y, rf_dia);
	}

	// setup the vector to record how many dots at different size(glPointSize()).
	if (m_starfield.drawMode == 3.0){ // drawing Dots' hollow sphere
		double gaussPercentage = 0.0;
		int totalDots = 0;
		int numOfDots = 0;
		int size = 1;

		mean = g_pList.GetVectorData("DOTS_SPHERE_PARA").at(2);
		sigma = g_pList.GetVectorData("DOTS_SPHERE_PARA").at(3);
		// Create a conversion factor to convert from degrees
		// into centimeters for OpenGL.
		mean = 2*tan(mean/2/180*PI)*(m_frustum.camera2screenDist);
		sigma = 2*tan(sigma/2/180*PI)*(m_frustum.camera2screenDist);
		int pixelWidth = 1280;	// viewport pixel width
//		if(RIG_ROOM == MOOG_164) pixelWidth = 782;	
//		else if(RIG_ROOM == MOOG_157) pixelWidth = 1010;

		// Change cm to pixel.
		mean = mean/m_frustum.screenWidth*pixelWidth;
		sigma = sigma/m_frustum.screenWidth*pixelWidth;

		dotSizeDistribute.clear();
		while(totalDots < m_totalStars){
			// use gaussian distribution find the percentage of total dots at certain size
			gaussPercentage = (1/sigma/sqrt(2*PI))*exp(-0.5*pow(((size-mean)/sigma),2.0));
			numOfDots = (int)(gaussPercentage*m_totalStars);
			// if we still have some dots left after we try different sizes,
			// then set the rest of dots at mean size
			if (size > mean && numOfDots < 1){ 
				dotSizeDistribute.at((int)(mean-0.5)) = dotSizeDistribute.at((int)(mean-0.5)) + m_totalStars - totalDots;
				//int tmp = dotSizeDistribute.at((int)(mean-0.5)); // for checking only
				break;
			}

			totalDots += numOfDots;
			if (totalDots >= m_totalStars){
				dotSizeDistribute.push_back(m_totalStars - totalDots + numOfDots);
				break;
			}
			else dotSizeDistribute.push_back(numOfDots);
			
			size++;
		}

		SetupCallList();
	}
}

GLvoid GLPanel::GenerateAllObjectStarFieldTexture()
{
	for(int i=0; i<NUM_OF_DYN_OBJ; i++)
	{	// need setup texture
		if(glDynObject[i]->GetObject().enable == 1.0 && glDynObject[i]->GetObject().bodyType == 2.0) 
			glDynObject[i]->CreateObjectTexture();
	}

	for(int i=0; i<NUM_OF_STATIC_OBJ; i++)
	{	// need setup texture
		if(glStaticObject[i]->GetObject().enable == 1.0 && glStaticObject[i]->GetObject().bodyType == 2.0) 
			glStaticObject[i]->CreateObjectTexture();
	}
}

GLvoid GLPanel::SetFrustum(Frustum frustum)
{
	m_frustum = frustum;
}


GLvoid GLPanel::SetStarField(StarField starfield)
{
	m_starfield = starfield;

	// Regenerate the starfield based on new data.
	if(computeStarField) GenerateStarField();
}


GLvoid GLPanel::CalculateStereoFrustum(GLfloat screenWidth, GLfloat screenHeight, GLfloat camera2screenDist,
									   GLfloat clipNear, GLfloat clipFar, GLfloat eyeSeparation,
									   GLfloat centerOffsetX, GLfloat centerOffsetY)
{
	GLfloat left, right, top, bottom;

	// Go to projection mode.
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();

	// We use similar triangles to solve for the left, right, top, and bottom of the clipping
	// plane.
	top = (clipNear / camera2screenDist) * (screenHeight / 2.0f - centerOffsetY);
	bottom = (clipNear / camera2screenDist) * (-screenHeight / 2.0f - centerOffsetY);
	right = (clipNear / camera2screenDist) * (screenWidth / 2.0f - eyeSeparation - centerOffsetX);
	left = (clipNear / camera2screenDist) * (-screenWidth / 2.0f - eyeSeparation - centerOffsetX);

	glFrustum(left, right, bottom, top, clipNear, clipFar);	
}


void GLPanel::OnSize(wxSizeEvent &event)
{
    // this is also necessary to update the context on some platforms
    wxGLCanvas::OnSize(event);

    // Set GL viewport (not called by wxGLCanvas::OnSize on all platforms...).
    int w, h;
    GetClientSize(&w, &h);
	if (GetContext())
    {
        SetCurrent();
        glViewport(0, 0, (GLint) w, (GLint) h);
    }
}


void GLPanel::OnPaint(wxPaintEvent &event)
{
	wxPaintDC dc(this);

	// Make sure that we render the OpenGL stuff.
	SetCurrent();
	Render();
	SwapBuffers();
}


void GLPanel::InitGL(void)
{
	glClearColor(0.0, 0.0, 0.0, 0.0);					// Set background to black.
	glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);
	glMatrixMode(GL_MODELVIEW);

#if USE_STEREO
	// Enable depth testing.
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LEQUAL);
#endif

#if USE_ANTIALIASING
	// Enable Antialiasing
	glEnable(GL_POINT_SMOOTH);
	glHint(GL_POINT_SMOOTH_HINT, GL_NICEST);
	glEnable(GL_POLYGON_SMOOTH);
	glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);	
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glHint(GL_POLYGON_SMOOTH_HINT, GL_NICEST);
#endif

    int w = PROJECTOR_NATIVE_HEIGHT*SCREEN_WIDTH/PROJECTOR_NATIVE_WIDTH, h = SCREEN_HEIGHT;  // New viewport settings.  JWN	glViewport((SCREEN_WIDTH-w)/2, (SCREEN_HEIGHT-h)/2, w, h);
	glViewport((SCREEN_WIDTH-w)/2, (SCREEN_HEIGHT-h)/2, w, h);

#if !DUAL_MONITORS
		glViewport(0,0,800, 870);
#endif
}

void GLPanel::RotationType(int val)
{
	m_rotationType = val;
}

void GLPanel::SetObjFieldTran(int objNum, double x, double y, double z)
{
	glDynObject[objNum]->SetObjFieldTran(x,y,z); //OpenGL coord
}

void GLPanel::SetAllObjRotAngle(int numFrame)
{
	for(int i=0; i<NUM_OF_DYN_OBJ; i++) 
	{	//When speed equals to zero, we don't need to set rotation angle.
		//In some case, we want the object in a constant rotation angle.
		if(glDynObject[i]->GetObject().rotSpeed != 0.0)
			glDynObject[i]->SetObjRotAngle(numFrame*glDynObject[i]->GetObject().rotSpeed/60);
	}
}

GLvoid GLPanel::DrawTargetObject(int targObj)
{
	glColor3d(	m_starfield.targRlum.at(targObj)*m_starfield.targLumMult.at(targObj), 
				m_starfield.targGlum.at(targObj)*m_starfield.targLumMult.at(targObj), 
				m_starfield.targBlum.at(targObj)*m_starfield.targLumMult.at(targObj));

	double t1 = m_starfield.targRlum.at(targObj)*m_starfield.targLumMult.at(targObj);
	double t2 = m_starfield.targGlum.at(targObj)*m_starfield.targLumMult.at(targObj);
	double t3 = m_starfield.targBlum.at(targObj)*m_starfield.targLumMult.at(targObj);

	// We need turn off GL_POLYGON_SMOOTH, because either polygon or rectangle
	// will have line inside with background color.
	glDisable(GL_POLYGON_SMOOTH);
	if (m_starfield.targShape.at(targObj) == SHAPE_ELLIPSE)
	{
		glEnableClientState(GL_VERTEX_ARRAY);
		//glVertexPointer(3, GL_FLOAT, 0, m_starfield.targVertex[targObj]);
		glVertexPointer(3, GL_FLOAT, 0, targVertex[targObj]);
		glDrawArrays(GL_POLYGON,0,DRAW_TARG_SLICES);
		glDisableClientState(GL_VERTEX_ARRAY);
	}
	else if(m_starfield.targShape.at(targObj) == SHAPE_RECTANGLE)
	{
		glRectf(-m_starfield.targXsize.at(targObj)/2.0, -m_starfield.targYsize.at(targObj)/2.0,
			m_starfield.targXsize.at(targObj)/2.0, m_starfield.targYsize.at(targObj)/2.0);
	}
	else if(m_starfield.targShape.at(targObj) == SHAPE_CROSS)
	{	
		double fpsize = m_starfield.targXsize.at(targObj)/2.0;
		// Draw fixation cross
		if(g_pList.GetVectorData("FP_ON").at(0) && fpsize>0.0)
		{    
			glBegin(GL_LINES);
				glVertex3f(fpsize, 0.0, 0.0);
				glVertex3f(-fpsize, 0.0, 0.0);
				glVertex3f(0.0, fpsize, 0.0);
				glVertex3f(0.0, -fpsize, 0.0);
			glEnd();
		}
	}
	glEnable(GL_POLYGON_SMOOTH);
}

GLvoid GLPanel::DrawObjectTargets(Point3 cameraTranslate)
{
	XYZ3 objOrigin;
	Point3 objPos, objPosOnScreen;
	Point3 cameraPos( cameraTranslate.x, cameraTranslate.y, m_frustum.camera2screenDist+cameraTranslate.z); //cm

	for(int i=0; i<NUM_OF_DYN_OBJ; i++)
	{
		if((int)g_pList.GetVectorData("OBJ_TARG_ENABLE").at(i)==1.0)
		{
			objOrigin = glDynObject[i]->GetObjOrigin(); //meter
			objPos.set(objOrigin.x*100 + cameraTranslate.x, objOrigin.y*100 + cameraTranslate.y, objOrigin.z*100 + cameraTranslate.z); //cm
			objPosOnScreen = FindPointByTwoPointForm(objPos, cameraPos, 0.0);

			glPushMatrix();
				//glTranslatef(objOrigin.x*100 + cameraTranslate.x, objOrigin.y*100 + cameraTranslate.y, objOrigin.z*100 + cameraTranslate.z); //cm
				glTranslatef(objPosOnScreen.x, objPosOnScreen.y, objPosOnScreen.z);
				DrawTargetObject(DRAW_TARG1);
			glPopMatrix();
		}
	}

	for(int i=0; i<NUM_OF_STATIC_OBJ; i++)
	{
		if((int)g_pList.GetVectorData("STATIC_OBJ_TARG_ENABLE").at(i)==1.0)
		{
			objOrigin = glStaticObject[i]->GetObjOrigin(); //meter
			objPos.set(objOrigin.x*100 + cameraTranslate.x, objOrigin.y*100 + cameraTranslate.y, objOrigin.z*100 + cameraTranslate.z); //cm
			objPosOnScreen = FindPointByTwoPointForm(objPos, cameraPos, 0.0);

			glPushMatrix();
				//glTranslatef(objOrigin.x*100 + cameraTranslate.x, objOrigin.y*100 + cameraTranslate.y, objOrigin.z*100 + cameraTranslate.z); //cm
				glTranslatef(objPosOnScreen.x, objPosOnScreen.y, objPosOnScreen.z);
				DrawTargetObject(DRAW_TARG2);
			glPopMatrix();
		}
	}
}

float GLPanel::RandomFloat(float min, float max)
{
	//first, use rand() to get an integer from 0-> RAND_MAX, rescale it to 0->1.0
	float	value = (float) rand() / (float)RAND_MAX;	//range 0->1.0
	//now, scale and shift into range min -> max
	value = min + (max - min)*value;
	return(value);
}

GLvoid GLPanel::SetStimParams()
{
	double CMPERDEG = CmPerDeg();
	double new_background_dens = g_pList.GetVectorData("BACKGROUND_DENS").at(0)/(CMPERDEG*CMPERDEG);
	double new_m_background_x_size = g_pList.GetVectorData("BACKGROUND_SIZE").at(0)*CMPERDEG;
	double new_m_background_y_size = g_pList.GetVectorData("BACKGROUND_SIZE").at(1)*CMPERDEG;
	double new_m_background_depth = DISPTODEPTH(g_pList.GetVectorData("BACKGROUND_DEPTH").at(0));
	double new_m_background_seed = g_pList.GetVectorData("DOTS_BIN_CORR_SEED").at(1);

	double new_dot_dens = g_pList.GetVectorData("PATCH_DENS").at(0)/(CMPERDEG*CMPERDEG);
	double new_m_dot_d_size = g_pList.GetVectorData("PATCH_DIAMETER").at(0)*CMPERDEG;
	double new_m_dot_x = g_pList.GetVectorData("PATCH_LOCATION").at(0)*CMPERDEG;
	double new_m_dot_y = g_pList.GetVectorData("PATCH_LOCATION").at(1)*CMPERDEG;
	double new_m_dot_depth = DISPTODEPTH(g_pList.GetVectorData("PATCH_DEPTH").at(0));
	double new_m_dot_depth_min = DISPTODEPTH(g_pList.GetVectorData("PATCH_DEPTH_RANGE").at(0));
	double new_m_dot_depth_max = DISPTODEPTH(g_pList.GetVectorData("PATCH_DEPTH_RANGE").at(1));
	double new_m_dot_bin_corr = g_pList.GetVectorData("PATCH_BIN_CORR").at(0);

	double new_m_fpx = g_pList.GetVectorData("FP_LOCATION").at(0)*CMPERDEG;
	double new_m_fpy = g_pList.GetVectorData("FP_LOCATION").at(1)*CMPERDEG;

	double new_m_pref_direction = g_pList.GetVectorData("PREF_DIRECTION").at(0);
	double new_m_binoc_disp = g_pList.GetVectorData("BINOC_DISP").at(0);
	double new_m_dot_seed = g_pList.GetVectorData("DOTS_BIN_CORR_SEED").at(0);

	// 11/10/2009 hkim. now we have random seed. Therefore, we don't need to compare parameter to 
	// determine if we generate new dots or not because now all things depend on seed and parameters.
	// so just re-generate patch and background according to given seed and parameters.
	// CAUTION: you should put srand() inside GenerateBackgroundDotField() or 
	// GenerateDotField() because these function are called repeatedly for coherence = 0 or for some
	// reason later. If we put srand() before calling these funtions.

//	if(new_background_dens != m_background_dens ||
//	   new_m_background_x_size != m_background_x_size ||
//	   new_m_background_y_size != m_background_y_size ||
//	   new_m_background_depth != m_background_depth ||
//	   new_m_background_seed != m_background_seed) 
	if (1)
	{
		m_background_dens = new_background_dens;
		m_background_x_size = new_m_background_x_size;
		m_background_y_size = new_m_background_y_size;
		m_background_depth = new_m_background_depth;
		m_background_seed = new_m_background_seed;

		// init random seed 
		srand((unsigned int) m_background_seed);
		if(computeStarField) GenerateBackgroundDotField();
	}
//	if(new_dot_dens != m_dot_dens ||
//	   new_m_dot_d_size != m_dot_d_size ||
//	   new_m_dot_x != m_dot_x ||
//	   new_m_dot_y != m_dot_y ||
//	   new_m_dot_depth != m_dot_depth ||
//	   new_m_dot_depth_min != m_dot_depth_min ||
//	   new_m_dot_depth_max != m_dot_depth_max ||
//	   new_m_dot_bin_corr != m_dot_bin_corr ||
//	   new_m_fpx != m_fpx ||
//	   new_m_fpy != m_fpy ||
//	   new_m_pref_direction != m_pref_direction ||
//	   new_m_binoc_disp != m_binoc_disp ||
//	   new_m_dot_seed  != m_dot_seed)
	if (1)
	{
		m_dot_dens = new_dot_dens;
		m_dot_d_size = new_m_dot_d_size;
		m_dot_x = new_m_dot_x;
		m_dot_y = new_m_dot_y;
		m_dot_depth = new_m_dot_depth;
		m_dot_depth_min = new_m_dot_depth_min;
		m_dot_depth_max = new_m_dot_depth_max;
		m_dot_bin_corr = new_m_dot_bin_corr;
		m_fpx = new_m_fpx;
		m_fpy = new_m_fpy;
		m_pref_direction = new_m_pref_direction;
		m_binoc_disp = new_m_binoc_disp;
		m_dot_seed  = new_m_dot_seed;
		srand((unsigned int) m_dot_seed);
		if(computeStarField) GenerateDotField();
	}
}

GLvoid GLPanel::GenerateDotDensities()
{
  	m_background_num = (GLint)(m_background_dens * m_background_x_size*m_background_y_size);
//#ifdef VERIFY_DEPTHDISC
//	m_dot_num = (GLint)(m_dot_dens * m_dot_d_size * m_dot_d_size) ;
//#else
	m_dot_num = (GLint)(m_dot_dens * PI*(m_dot_d_size/2.0)*(m_dot_d_size/2.0));
//#endif
}

GLvoid GLPanel::GenerateBackgroundDotField()
{
	int i;
	double sd0, sd1, sd2;

	// Delete the old Star array if needed.
	if (m_backgroundArray != NULL) {
		delete [] m_backgroundArray;
	}
	GenerateDotDensities();

	// Allocate the Star array.
	m_backgroundArray = new Star[m_background_num];
    sd0 = m_background_x_size;
	sd1 = m_background_y_size;
	sd2 = m_background_depth;
	for (i = 0; i < m_background_num; i++) {
		// Pick a random point for our dot.
		m_backgroundArray[i].x[0] = (double)rand()/(double)RAND_MAX*sd0 - sd0/2.0;
		m_backgroundArray[i].x[1] = (double)rand()/(double)RAND_MAX*sd1 - sd1/2.0;
		m_backgroundArray[i].x[2] = sd2;
	}


	
	
}

GLvoid GLPanel::GenerateDotField()
{
	GenerateDotField2();
	return;

	int i;
	double x,y,r;
	//double view_dist = g_pList.GetVectorData("VIEW_DIST").at(0);
	//f.camera2screenDist = CENTER2SCREEN - eyeOffsets.at(1) - headCenter.at(1);	// Distance from monkey to screen.
	double view_dist = m_frustum.camera2screenDist;
	double pref_direction = g_pList.GetVectorData("PREF_DIRECTION").at(0);
	double dot_depth;
	double dot_depth_signal = g_pList.GetVectorData("PATCH_DEPTH").at(0);
	double dot_depth_min = g_pList.GetVectorData("PATCH_DEPTH_RANGE").at(0);
	double dot_depth_max = g_pList.GetVectorData("PATCH_DEPTH_RANGE").at(1);
	double dot_bin_corr = g_pList.GetVectorData("PATCH_BIN_CORR").at(0);
	double stereo_mp_diff = g_pList.GetVectorData("STEREO_MP_DIFF").at(0);


	// Delete the old Star array if needed.
	if(m_dotArray != NULL) delete [] m_dotArray;
	if(m_dotArray_init != NULL) delete [] m_dotArray_init;
	GenerateDotDensities();

	// Allocate the Star array.
	m_dotArray = new Star[m_dot_num+2];  // Include the +2 to hold marker dots
	m_dotArray_init = new Star[m_dot_num+2];  // Include the +2 to hold marker dots
	
	// Rotate patch location for mapping to vertical cylinder.  Final patch will then be unrotated.
	double true_patch_r = sqrt(pow(m_dot_x,2)+pow(m_dot_y,2));
	double true_patch_t = atan2(m_dot_y,m_dot_x);  // in radians
	double new_patch_x = true_patch_r*cos(true_patch_t-pref_direction*PI/180);
	double new_patch_y = true_patch_r*sin(true_patch_t-pref_direction*PI/180);

	// Create frontoparallel patch in the plane of the screen
	double sd0 = m_dot_d_size;
	for(i=0;i<m_dot_num;i++){
		// Pick a random point for our dot.
		x = (double)rand()/(double)RAND_MAX*sd0 - sd0/2.0;
		y = (double)rand()/(double)RAND_MAX*sd0 - sd0/2.0;
		//x = 0; // (double)rand()/(double)RAND_MAX*sd0 - sd0/2.0; // for debug
		//y = (double) i / (double) m_dot_num * sd0 - sd0/2.0; // (double)rand()/(double)RAND_MAX*sd0 - sd0/2.0;
		//x = ((double)i/m_dot_num -0.5)*0.5 * sd0;
		//y = ((double)i/m_dot_num -0.5 )*0.5 * sd0;
		
		if(sqrt(x*x+y*y)>sd0/2.0) i--;
		else{
			m_dotArray[i].x[0] = x+new_patch_x+m_fpx;
			m_dotArray[i].x[1] = y+new_patch_y+m_fpy;
			m_dotArray[i].x[2] = 0.0;

			m_dotArray_init[i].x[0] = x+new_patch_x+m_fpx + stereo_mp_diff*CmPerDeg();
			m_dotArray_init[i].x[1] = y+new_patch_y+m_fpy;
			m_dotArray_init[i].x[2] = 0.0;
		}
	}

	//// Set up marker dots for computation of m_VM_angle.  Not used.
	//m_dotArray[m_dot_num].xyz[0] = sd0/2.0+new_patch_x+m_fpx;  // Right marker dot
	//m_dotArray[m_dot_num].xyz[1] = new_patch_y+m_fpy;
	//m_dotArray[m_dot_num].xyz[2] = 0.0;
	//m_dotArray[m_dot_num+1].xyz[0] = -sd0/2.0+new_patch_x+m_fpx;  // Left marker dot
	//m_dotArray[m_dot_num+1].xyz[1] = new_patch_y+m_fpy;
	//m_dotArray[m_dot_num+1].xyz[2] = 0.0;

	// Move the patch to the cylinder and unrotate.
	// We now use binoc_disp to paint the patch on the cylinder.
	// The VM circle remains centered, but the ray now projects from the cyclopean(0,1) or left(2) or right(3) eye.
	double eye_shift;  // Used to calculate the ray that intersects the VM circle
	//switch((int)g_pList.GetVectorData("BINOC_DISP").at(0)){
	//	case 0:
	//	case 1: eye_shift = 0.0;  break;
	//	case 2: eye_shift = -g_pList.GetVectorData("IO_DIST").at(0)/2.0;  break;
	//	case 3: eye_shift = g_pList.GetVectorData("IO_DIST").at(0)/2.0;
	//}
	eye_shift = 0.0;  // No longer using a shifted eye.  All patches are painted based on the cyclopean eye.
	// set noise depth criterion
	int bin_corr_crit = (int)(0.01* dot_bin_corr * (float)RAND_MAX );

	for(i=0;i<m_dot_num;i++){  // Loop through all patch dots and NOT marker dots (+2 no longer used)
		x = m_dotArray[i].x[0];
		y = m_dotArray[i].x[1];

		// set dot depth using bin. corr. hkim
		if (rand() > bin_corr_crit)
			dot_depth = DISPTODEPTH( RandomFloat(dot_depth_min, dot_depth_max) );
		else
			dot_depth = DISPTODEPTH(dot_depth_signal);
		
		// Drop the point to the horizontal meridian and intersect it with the VM circle.
		r = (view_dist - dot_depth)/2.0;
		double x1 = r;
		double y1 = eye_shift;  // Align the ray with the eye (cyclopean, left, right) we're displaying to.
		double x2 = r - view_dist;
		double y2 = x;
		double dx = x2-x1;  // Should always = view_dist;
		double dy = y2-y1;  // Should always = x;
		double dr = sqrt(pow(dx,2)+pow(dy,2));
		double D = x1*y2 - x2*y1;
		double sgn = 1.0;
		double xi = (D*dy + sgn*dx*sqrt(pow(r,2)*pow(dr,2)-pow(D,2)))/pow(dr,2) + r + dot_depth;  // xi is in z-plane
		double yi = (-D*dx + dy*sqrt(pow(r,2)*pow(dr,2)-pow(D,2)))/pow(dr,2);  // yi is in x-plane
		x = yi;  // PAINTED X
		m_dotArray[i].x[2] = xi;  // PAINTED Z, FINAL Z
		// Raise it back up using similar triangles.
		double newH = sqrt(pow(yi,2) + pow(view_dist-xi,2));
		y = newH*y/dr;  // PAINTED Y
		// Unrotate based on pref_direction.
		double dot_r = sqrt(pow(x,2)+pow(y,2));
		double dot_t = atan2(y,x);  // in radians
        m_dotArray[i].x[0] = dot_r*cos(dot_t+pref_direction*PI/180);  // FINAL X
        m_dotArray[i].x[1] = dot_r*sin(dot_t+pref_direction*PI/180);  // FINAL Y


		//////////////////////////////////////////
		// for incongruent stimulus.
		//////////////////////////////////////////
		x = m_dotArray_init[i].x[0];
		y = m_dotArray_init[i].x[1];

		
		// set dot depth using bin. corr. hkim - DO NOT CHANGE FOR INCONGRUENT DOTS
//		if (rand() > bin_corr_crit)
//			dot_depth = DISPTODEPTH( RandomFloat(dot_depth_min, dot_depth_max) );
//		else
//			dot_depth = DISPTODEPTH(dot_depth_signal);

		// Drop the point to the horizontal meridian and intersect it with the VM circle.
		r = (view_dist - dot_depth)/2.0;
		x1 = r;
		 y1 = eye_shift;  // Align the ray with the eye (cyclopean, left, right) we're displaying to.
		 x2 = r - view_dist;
		 y2 = x;
		 dx = x2-x1;  // Should always = view_dist;
		 dy = y2-y1;  // Should always = x;
		 dr = sqrt(pow(dx,2)+pow(dy,2));
		 D = x1*y2 - x2*y1;
		 sgn = 1.0;
		 xi = (D*dy + sgn*dx*sqrt(pow(r,2)*pow(dr,2)-pow(D,2)))/pow(dr,2) + r + dot_depth;  // xi is in z-plane
		 yi = (-D*dx + dy*sqrt(pow(r,2)*pow(dr,2)-pow(D,2)))/pow(dr,2);  // yi is in x-plane
		x = yi;  // PAINTED X
		m_dotArray_init[i].x[2] = xi;  // PAINTED Z, FINAL Z
		// Raise it back up using similar triangles.
		 newH = sqrt(pow(yi,2) + pow(view_dist-xi,2));
		y = newH*y/dr;  // PAINTED Y
		// Unrotate based on pref_direction.
		 dot_r = sqrt(pow(x,2)+pow(y,2));
		 dot_t = atan2(y,x);  // in radians
        m_dotArray_init[i].x[0] = dot_r*cos(dot_t+pref_direction*PI/180);  // FINAL X
        m_dotArray_init[i].x[1] = dot_r*sin(dot_t+pref_direction*PI/180);  // FINAL Y
	}
	// Use marker dots to compute m_VM_angle before returning.  Not used.
	//r = view_dist - (view_dist - dot_depth)/2.0;
	//double VM_angleR = atan2(m_dotArray[m_dot_num].xyz[2]-r,m_dotArray[m_dot_num].xyz[0]);
	//double VM_angleL = atan2(m_dotArray[m_dot_num+1].xyz[2]-r,m_dotArray[m_dot_num+1].xyz[0]);
	//m_VM_angle = fabs(VM_angleR - VM_angleL);

}

GLvoid GLPanel::GenerateDotField2()
{
int i;
	double x,y;//,r;
	//double view_dist = g_pList.GetVectorData("VIEW_DIST").at(0);
	//f.camera2screenDist = CENTER2SCREEN - eyeOffsets.at(1) - headCenter.at(1);	// Distance from monkey to screen.
	double view_dist = m_frustum.camera2screenDist;
	double pref_direction = g_pList.GetVectorData("PREF_DIRECTION").at(0);
	//double dot_depth;
	double dot_depth_signal = g_pList.GetVectorData("PATCH_DEPTH").at(0);
	double dot_depth_min = g_pList.GetVectorData("PATCH_DEPTH_RANGE").at(0);
	double dot_depth_max = g_pList.GetVectorData("PATCH_DEPTH_RANGE").at(1);
	double dot_bin_corr = g_pList.GetVectorData("PATCH_BIN_CORR").at(0);
	double stereo_mp_diff = g_pList.GetVectorData("STEREO_MP_DIFF").at(0);

	// Delete the old Star array if needed.
	if(m_dotArray != NULL) delete [] m_dotArray;
	if(m_dotArray_init != NULL) delete [] m_dotArray_init;
	GenerateDotDensities();

	// Allocate the Star array.
	m_dotArray = new Star[m_dot_num+2];  // Include the +2 to hold marker dots
	m_dotArray_init = new Star[m_dot_num+2];  // Include the +2 to hold marker dots
	
	// Rotate patch location for mapping to vertical cylinder.  Final patch will then be unrotated.
	double true_patch_r = sqrt(pow(m_dot_x,2)+pow(m_dot_y,2));
	double true_patch_t = atan2(m_dot_y,m_dot_x);  // in radians
	double new_patch_x = true_patch_r*cos(true_patch_t-pref_direction*PI/180);
	double new_patch_y = true_patch_r*sin(true_patch_t-pref_direction*PI/180);
	// Don't rotate patch center here. I'll rotate every dots after making them to modularize routine.
	new_patch_x = m_dot_x;
	new_patch_y = m_dot_y;

	// Create frontoparallel patch in the plane of the screen
	double sd0 = m_dot_d_size;
	for(i=0;i<m_dot_num;i++){
		// Pick a random point for our dot.
		x = (double)rand()/(double)RAND_MAX*sd0 - sd0/2.0;
		y = (double)rand()/(double)RAND_MAX*sd0 - sd0/2.0;
		//x = 0; // (double)rand()/(double)RAND_MAX*sd0 - sd0/2.0; // for debug
		//y = (double) i / (double) m_dot_num * sd0 - sd0/2.0; // (double)rand()/(double)RAND_MAX*sd0 - sd0/2.0;
		//x = ((double)i/m_dot_num -0.5)*0.5 * sd0;
		//y = ((double)i/m_dot_num -0.5 )*0.5 * sd0;
		
		if(sqrt(x*x+y*y)>sd0/2.0) i--;
		else{
			m_dotArray[i].x[0] = x+new_patch_x+m_fpx;
			m_dotArray[i].x[1] = y+new_patch_y+m_fpy;
			m_dotArray[i].x[2] = 0.0;

			m_dotArray_init[i].x[0] = x+new_patch_x+m_fpx + stereo_mp_diff*CmPerDeg();
			m_dotArray_init[i].x[1] = y+new_patch_y+m_fpy;
			m_dotArray_init[i].x[2] = 0.0;
		}
	}

	//// Set up marker dots for computation of m_VM_angle.  Not used.
	//m_dotArray[m_dot_num].xyz[0] = sd0/2.0+new_patch_x+m_fpx;  // Right marker dot
	//m_dotArray[m_dot_num].xyz[1] = new_patch_y+m_fpy;
	//m_dotArray[m_dot_num].xyz[2] = 0.0;
	//m_dotArray[m_dot_num+1].xyz[0] = -sd0/2.0+new_patch_x+m_fpx;  // Left marker dot
	//m_dotArray[m_dot_num+1].xyz[1] = new_patch_y+m_fpy;
	//m_dotArray[m_dot_num+1].xyz[2] = 0.0;

	// Move the patch to the cylinder and unrotate.
	// We now use binoc_disp to paint the patch on the cylinder.
	// The VM circle remains centered, but the ray now projects from the cyclopean(0,1) or left(2) or right(3) eye.
	double eye_shift;  // Used to calculate the ray that intersects the VM circle
	//switch((int)g_pList.GetVectorData("BINOC_DISP").at(0)){
	//	case 0:
	//	case 1: eye_shift = 0.0;  break;
	//	case 2: eye_shift = -g_pList.GetVectorData("IO_DIST").at(0)/2.0;  break;
	//	case 3: eye_shift = g_pList.GetVectorData("IO_DIST").at(0)/2.0;
	//}
	eye_shift = 0.0;  // No longer using a shifted eye.  All patches are painted based on the cyclopean eye.
	// set noise depth criterion
	int bin_corr_crit = (int)(0.01* dot_bin_corr * (float)RAND_MAX );

	Project2DPlaneToCylinder(m_dotArray, m_dot_num, pref_direction, eye_shift, view_dist, dot_depth_signal, 
		bin_corr_crit, dot_depth_min, dot_depth_max);

	Project2DPlaneToCylinder(m_dotArray_init, m_dot_num, pref_direction, eye_shift, view_dist, dot_depth_signal, 
		bin_corr_crit, dot_depth_min, dot_depth_max);

	Project2DPlaneToCylinder(m_backgroundArray, m_background_num, pref_direction, eye_shift, view_dist, m_background_depth, 
		RAND_MAX, 0, 0);
}

GLvoid GLPanel::Project2DPlaneToCylinder(Star *dotarray, GLint dot_num, double pref_direction, double eye_shift, double view_dist,
								double dot_depth_signal, int bin_corr_crit, double dot_depth_min, double dot_depth_max)
{
	int i;
	double x, y;
	double dot_r, dot_t;	// rotating cylinder
	double dot_depth, r;	// vm circle projection

	for(i=0;i<dot_num;i++){  // Loop through all patch dots and NOT marker dots (+2 no longer used)

		x = dotarray[i].x[0];
		y = dotarray[i].x[1];

		// Rotate based on pref_direction.
		dot_r = sqrt(pow(x,2)+pow(y,2));
		dot_t = atan2(y,x);  // in radians
        x = dot_r*cos(dot_t-pref_direction*PI/180);  // FINAL X
        y = dot_r*sin(dot_t-pref_direction*PI/180);  // FINAL Y

		// set dot depth using bin. corr. hkim
		if (rand() > bin_corr_crit)
			dot_depth = DISPTODEPTH( RandomFloat(dot_depth_min, dot_depth_max) );
		else
			dot_depth = DISPTODEPTH(dot_depth_signal);
		
		// Drop the point to the horizontal meridian and intersect it with the VM circle.
		r = (view_dist - dot_depth)/2.0;
		double x1 = r;
		double y1 = eye_shift;  // Align the ray with the eye (cyclopean, left, right) we're displaying to.
		double x2 = r - view_dist;
		double y2 = x;
		double dx = x2-x1;  // Should always = view_dist;
		double dy = y2-y1;  // Should always = x;
		double dr = sqrt(pow(dx,2)+pow(dy,2));
		double D = x1*y2 - x2*y1;
		double sgn = 1.0;
		double xi = (D*dy + sgn*dx*sqrt(pow(r,2)*pow(dr,2)-pow(D,2)))/pow(dr,2) + r + dot_depth;  // xi is in z-plane
		double yi = (-D*dx + dy*sqrt(pow(r,2)*pow(dr,2)-pow(D,2)))/pow(dr,2);  // yi is in x-plane
		x = yi;  // PAINTED X
		dotarray[i].x[2] = xi;  // PAINTED Z, FINAL Z
		// Raise it back up using similar triangles.
		double newH = sqrt(pow(yi,2) + pow(view_dist-xi,2));
		y = newH*y/dr;  // PAINTED Y
		// Unrotate based on pref_direction.
		dot_r = sqrt(pow(x,2)+pow(y,2));
		dot_t = atan2(y,x);  // in radians
        dotarray[i].x[0] = dot_r*cos(dot_t+pref_direction*PI/180);  // FINAL X
        dotarray[i].x[1] = dot_r*sin(dot_t+pref_direction*PI/180);  // FINAL Y
	}
}

// Star *
GLvoid GLPanel::CutOutBGDotsByCylinder(GLfloat *dotarray, GLint dot_num, double pref_direction, double eye_shift, double view_dist,
								double dot_depth_signal, int bin_corr_crit, double dot_depth_min, double dot_depth_max, 
								double rf_x, double rf_y, double rf_dia)
{
	int i;
	double x, y, z, px, py, scr_x, scr_y, scr_z;
	double dot_r, dot_t;	// rotating cylinder
	double dot_depth, vm0_depth, r;	// vm circle projection
    bool bDot = (g_pList.GetVectorData("DRAW_MODE").at(0) != 1.0);			// dot or triangle
	double cut_radius = g_pList.GetVectorData("CUTOUT_RADIUS").at(0);		// use this for cutout crit.
	// CutOffMode: 0: no cutoff 1: 1D planar 2: 2D planar 3: cylinder 1D , 4: cylinder 2D (3,4 are obsolute)
	// (previously, it was 0: no, 1: 1D planar, 2: 1D cylinder, DotDepthMode=1, 3: 1D cylinder,DotDepthMode=2)
	int CutOffMode; 
	int DotDepthMode; // 1=random cube 2=cylinder depth range
	double ts0, ts1;
	
	CutOffMode = (int) g_pList.GetVectorData("CUTOUT_STARFIELD_MODE").at(0);
	DotDepthMode = (int) g_pList.GetVectorData("STAR_LOCATION").at(0);

	// Create a conversion factor to convert from TEMPO degrees
	// into centimeters for OpenGL.
	double deg2cm = tan(PI/180)*(CENTER2SCREEN -
		g_pList.GetVectorData("HEAD_CENTER").at(1) - g_pList.GetVectorData("EYE_OFFSETS").at(1));

	x = rf_x*deg2cm; // dotarray[i].x[0];
	y = rf_y*deg2cm; // dotarray[i].x[1];

	// Rotate based on pref_direction.
	dot_r = sqrt(pow(x,2)+pow(y,2));
	dot_t = atan2(y,x);  // in radians
    x = dot_r*cos(dot_t-pref_direction*PI/180);  // FINAL X
	y = dot_r*sin(dot_t-pref_direction*PI/180);  // FINAL Y

	vm0_depth = DISPTODEPTH(0);

	if (CutOffMode == 1 || CutOffMode == 2) {	// cylinder cutoff
		// use screen coordinate
		px=x;
		py=y;
	} else {			// planer cutoff
		// Drop the point to the horizontal meridian and intersect it with the VM circle.
		r = (view_dist - vm0_depth)/2.0;
		double x1 = r;
		double y1 = eye_shift;  // Align the ray with the eye (cyclopean, left, right) we're displaying to.
		double x2 = r - view_dist;
		double y2 = x;
		double dx = x2-x1;  // Should always = view_dist;
		double dy = y2-y1;  // Should always = x;
		double dr = sqrt(pow(dx,2)+pow(dy,2));
		double D = x1*y2 - x2*y1;
		double sgn = 1.0;
		double xi = (D*dy + sgn*dx*sqrt(pow(r,2)*pow(dr,2)-pow(D,2)))/pow(dr,2) + r + vm0_depth;  // xi is in z-plane
		double yi = (-D*dx + dy*sqrt(pow(r,2)*pow(dr,2)-pow(D,2)))/pow(dr,2);  // yi is in x-plane
		px = yi;  // PAINTED X
		//z = xi;  // PAINTED Z, FINAL Z
		// Raise it back up using similar triangles.
		double newH = sqrt(pow(yi,2) + pow(view_dist-xi,2));
		py = newH*y/dr;  // PAINTED Y
	}

	for(i=0;i<dot_num;i++){  // Loop through all patch dots and NOT marker dots (+2 no longer used)

		// get dot coordinates
		if (bDot) {
			x=dotarray[i*3]; y = dotarray[i*3+1]; z = dotarray[i*3+2];
		} else {
			x=dotarray[i*9]; y = dotarray[i*9+1]; z = dotarray[i*9+2];
		}

		// Rotate each dot based on pref_direction.
		dot_r = sqrt(pow(x,2)+pow(y,2));
		dot_t = atan2(y,x);  // in radians
        x = dot_r*cos(dot_t-pref_direction*PI/180);  // FINAL X
        y = dot_r*sin(dot_t-pref_direction*PI/180);  // FINAL Y

		vm0_depth = DISPTODEPTH(0);
		
		// project 3D dot onto the screen. this is necessary because both cylinder and planer assumes
		// dots on the screen
		scr_x = view_dist / (view_dist - z) * x;
		scr_y = view_dist / (view_dist - z) * y;
		
		if (CutOffMode == 1 || CutOffMode == 2) {			// planer cutoff
			x = scr_x;
			y = scr_y;			
		} else if (CutOffMode == 3 || CutOffMode == 4) {	// cylinder cutoff
			// Drop the point to the horizontal meridian and intersect it with the VM circle.
			r = (view_dist - vm0_depth)/2.0;
			double x1 = r;
			double y1 = eye_shift;  // Align the ray with the eye (cyclopean, left, right) we're displaying to.
			double x2 = r - view_dist;
			double y2 = scr_x;
			double dx = x2-x1;  // Should always = view_dist;
			double dy = y2-y1;  // Should always = x;
			double dr = sqrt(pow(dx,2)+pow(dy,2));
			double D = x1*y2 - x2*y1;
			double sgn = 1.0;
			double xi = (D*dy + sgn*dx*sqrt(pow(r,2)*pow(dr,2)-pow(D,2)))/pow(dr,2) + r + vm0_depth;  // xi is in z-plane
			double yi = (-D*dx + dy*sqrt(pow(r,2)*pow(dr,2)-pow(D,2)))/pow(dr,2);  // yi is in x-plane
			x = yi;  // PAINTED X
			z = xi;  // PAINTED Z, FINAL Z
			// Raise it back up using similar triangles.
			double newH = sqrt(pow(yi,2) + pow(view_dist-xi,2));
			y = newH*scr_y/dr;  // PAINTED Y
		}

		// compare location with criteria and remove dot
		if ( 
			( (CutOffMode == 1 || CutOffMode == 3) && (y < (py + cut_radius*deg2cm)) && y > (py - cut_radius*deg2cm) ) || 
			( (CutOffMode == 2 || CutOffMode == 4) && (pow((y - py),2) + pow((x - px),2)) < pow(cut_radius*deg2cm,2) ) 
			) {
			// set the dot location to zero when it is located within range
			// later it can be implemented to really remove dots.
			if (bDot) {
				dotarray[i*3]=0; dotarray[i*3+1]=0; dotarray[i*3+2]=50;
			} else {
				dotarray[i*9]=0; dotarray[i*9+1]=0; dotarray[i*9+2]=50;
				dotarray[i*9+3]=0; dotarray[i*9+4]=0; dotarray[i*9+5]=50;
				dotarray[i*9+6]=0; dotarray[i*9+7]=0; dotarray[i*9+8]=50;
			}
		} else {	// visible dot
			if (DotDepthMode == 1) {	// leave original location of dot. do noghting

			} else if (DotDepthMode == 2) {	// cylinder depth range
				// now, use cylinder projection to randomize depth of the scr_x, scr_y dot
				dot_depth = DISPTODEPTH( RandomFloat(dot_depth_min, dot_depth_max) );
				// Drop the point to the horizontal meridian and intersect it with the VM circle.
				r = (view_dist - dot_depth)/2.0;
				double x1 = r;
				double y1 = eye_shift;  // Align the ray with the eye (cyclopean, left, right) we're displaying to.
				double x2 = r - view_dist;
				double y2 = scr_x;
				double dx = x2-x1;  // Should always = view_dist;
				double dy = y2-y1;  // Should always = x;
				double dr = sqrt(pow(dx,2)+pow(dy,2));
				double D = x1*y2 - x2*y1;
				double sgn = 1.0;
				double xi = (D*dy + sgn*dx*sqrt(pow(r,2)*pow(dr,2)-pow(D,2)))/pow(dr,2) + r + dot_depth;  // xi is in z-plane
				double yi = (-D*dx + dy*sqrt(pow(r,2)*pow(dr,2)-pow(D,2)))/pow(dr,2);  // yi is in x-plane
				x = yi;  // PAINTED X
				z = xi;  // PAINTED Z, FINAL Z
				// Raise it back up using similar triangles.
				double newH = sqrt(pow(yi,2) + pow(view_dist-xi,2));
				y = newH*scr_y/dr;  // PAINTED Y

				// Unrotate based on pref_direction.
				dot_r = sqrt(pow(x,2)+pow(y,2));
				dot_t = atan2(y,x);  // in radians
				x = dot_r*cos(dot_t+pref_direction*PI/180);  // FINAL X
				y = dot_r*sin(dot_t+pref_direction*PI/180);  // FINAL Y
				
				ts0 = m_starfield.triangle_size[0];
				ts1 = m_starfield.triangle_size[1];

				if (bDot) {
					dotarray[i*3]=x; dotarray[i*3+1]=y; dotarray[i*3+2]=z;
				} else {
					dotarray[i*9]=x - ts0/2.0; dotarray[i*9+1]=y - ts1/2.0; dotarray[i*9+2]=z;
					dotarray[i*9+3]=x; dotarray[i*9+4]=y + ts1/2.0; dotarray[i*9+5]=z;
					dotarray[i*9+6]=x + ts0/2.0; dotarray[i*9+7]=y - ts1/2.0; dotarray[i*9+8]=z;
				}
			}
		}
	} // end of for(i=0;i<dot_num;i++){
}

GLvoid GLPanel::DrawIncongDotFields()
{
int i,j;
	//double view_dist = g_pList.GetVectorData("VIEW_DIST").at(0);
	//f.camera2screenDist = CENTER2SCREEN - eyeOffsets.at(1) - headCenter.at(1);	// Distance from monkey to screen.
	double view_dist = m_frustum.camera2screenDist;

	// Don't try to mess with an unallocated array.
	if (m_dotArray_init == NULL || m_backgroundArray == NULL){
		return;
	}
	if(g_pList.GetVectorData("BACKGROUND_ON").at(0))
	{  // BACKGROUND_ON really means "Dots On"
		// Background
		for (i = 0; i < m_background_num; i++){
			glBegin(GL_POINTS);
				glVertex3d(m_backgroundArray[i].x[0], m_backgroundArray[i].x[1], m_backgroundArray[i].x[2]);
			glEnd();
		}
		// Draw Dot field
		if(g_pList.GetVectorData("GLU_LOOK_AT").at(0)==3.0){  // Spin the cylinder in BD condition
			glPushMatrix();
			glTranslated(0.0, 0.0, view_dist - (view_dist - m_dot_depth)/2.0);
			// Set up axis of rotation.  Default vertical cylinder would be 0.0, 1.0.
			double axis_t = (m_pref_direction+90.0)*PI/180;
			double dist = sqrt(pow(m_Lateral,2) + pow(m_Heave,2));  // Movement trajectory (windowed sin wave)
			double sgn = fabs(atan2(-m_Heave,m_Lateral)+PI - m_pref_direction*PI/180)<1?1.0:-1.0;
			double rot = sgn*dist*g_pList.GetVectorData("PATCH_DRIFT").at(0);
			glRotated(sgn*dist*g_pList.GetVectorData("PATCH_DRIFT").at(0), cos(axis_t), sin(axis_t), 0.0);
			glTranslated(0.0, 0.0, -(view_dist - (view_dist - m_dot_depth)/2.0));
		}
		for (i = 0; i < m_dot_num; i++){
			glBegin(GL_POINTS);
				glVertex3d(m_dotArray_init[i].x[0], m_dotArray_init[i].x[1], m_dotArray_init[i].x[2]);
			glEnd();
		}
		if(g_pList.GetVectorData("GLU_LOOK_AT").at(0)==3.0)	glPopMatrix();
	}

	glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
	// Draw fixation cross - moved to separate function - DrawFixation();
	//if(g_pList.GetVectorData("FP_ON").at(0) && fpsize>0.0)
	//{    
	//	double FPgain = g_pList.GetVectorData("PURSUIT_GAIN").at(0);   // Compensates for poor pursuit gain.
	//	glBegin(GL_LINES);
	//		glVertex3f(fpsize+m_fpx-(m_Lateral/FPgain-m_Lateral), m_fpy+(m_Heave/FPgain-m_Heave),0.0);
	//		glVertex3f(-fpsize+m_fpx-(m_Lateral/FPgain-m_Lateral), m_fpy+(m_Heave/FPgain-m_Heave),0.0);
	//		glVertex3f(m_fpx-(m_Lateral/FPgain-m_Lateral), fpsize+m_fpy+(m_Heave/FPgain-m_Heave),0.0);
	//		glVertex3f(m_fpx-(m_Lateral/FPgain-m_Lateral), -fpsize+m_fpy+(m_Heave/FPgain-m_Heave),0.0);
	//	glEnd();
	//}

	if(g_pList.GetVectorData("ALIGN_ON").at(0))
	{  // Alignment grid for centering/scaling.
		glBegin(GL_POINTS);  
			for(i=-20; i<21; i+=5){
				for(j=-20; j<21; j+=5){
					glVertex3d(i, j, 0.0);
				}
			}
		glEnd();
	}
}

GLvoid GLPanel::DrawDotFields()
{
	int i,j;
	//double view_dist = g_pList.GetVectorData("VIEW_DIST").at(0);
	//f.camera2screenDist = CENTER2SCREEN - eyeOffsets.at(1) - headCenter.at(1);	// Distance from monkey to screen.
	double view_dist = m_frustum.camera2screenDist;

	// Don't try to mess with an unallocated array.
	if (m_dotArray == NULL || m_backgroundArray == NULL){
		return;
	}
	if(g_pList.GetVectorData("BACKGROUND_ON").at(0))
	{  // BACKGROUND_ON really means "Dots On"
		// Background
		for (i = 0; i < m_background_num; i++){
			glBegin(GL_POINTS);
				glVertex3d(m_backgroundArray[i].x[0], m_backgroundArray[i].x[1], m_backgroundArray[i].x[2]);
			glEnd();
		}
		// Draw Dot field
		if(g_pList.GetVectorData("GLU_LOOK_AT").at(0)==3.0){  // Spin the cylinder in BD condition
			glPushMatrix();
			glTranslated(0.0, 0.0, view_dist - (view_dist - m_dot_depth)/2.0);
			// Set up axis of rotation.  Default vertical cylinder would be 0.0, 1.0.
			double axis_t = (m_pref_direction+90.0)*PI/180;
			double dist = sqrt(pow(m_Lateral,2) + pow(m_Heave,2));  // Movement trajectory (windowed sin wave)
			double sgn = fabs(atan2(-m_Heave,m_Lateral)+PI - m_pref_direction*PI/180)<1?1.0:-1.0;
			double rot = sgn*dist*g_pList.GetVectorData("PATCH_DRIFT").at(0);
			glRotated(sgn*dist*g_pList.GetVectorData("PATCH_DRIFT").at(0), cos(axis_t), sin(axis_t), 0.0);
			glTranslated(0.0, 0.0, -(view_dist - (view_dist - m_dot_depth)/2.0));
		}
		for (i = 0; i < m_dot_num; i++){
			glBegin(GL_POINTS);
				glVertex3d(m_dotArray[i].x[0], m_dotArray[i].x[1], m_dotArray[i].x[2]);
			glEnd();
		}
		if(g_pList.GetVectorData("GLU_LOOK_AT").at(0)==3.0)	glPopMatrix();
	}

	glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
	// Draw fixation cross - moved to separate function - DrawFixation();
	//if(g_pList.GetVectorData("FP_ON").at(0) && fpsize>0.0)
	//{    
	//	double FPgain = g_pList.GetVectorData("PURSUIT_GAIN").at(0);   // Compensates for poor pursuit gain.
	//	glBegin(GL_LINES);
	//		glVertex3f(fpsize+m_fpx-(m_Lateral/FPgain-m_Lateral), m_fpy+(m_Heave/FPgain-m_Heave),0.0);
	//		glVertex3f(-fpsize+m_fpx-(m_Lateral/FPgain-m_Lateral), m_fpy+(m_Heave/FPgain-m_Heave),0.0);
	//		glVertex3f(m_fpx-(m_Lateral/FPgain-m_Lateral), fpsize+m_fpy+(m_Heave/FPgain-m_Heave),0.0);
	//		glVertex3f(m_fpx-(m_Lateral/FPgain-m_Lateral), -fpsize+m_fpy+(m_Heave/FPgain-m_Heave),0.0);
	//	glEnd();
	//}

	if(g_pList.GetVectorData("ALIGN_ON").at(0))
	{  // Alignment grid for centering/scaling.
		glBegin(GL_POINTS);  
			for(i=-20; i<21; i+=5){
				for(j=-20; j<21; j+=5){
					glVertex3d(i, j, 0.0);
				}
			}
		glEnd();
	}
}

//#define CMPERDEG  g_pList.GetVectorData("VIEW_DIST").at(0)*PI/180.0
double GLPanel::CmPerDeg()
{
	return m_frustum.camera2screenDist*PI/180.0;
}

//#define DISPTODEPTH(disp)  (g_pList.GetVectorData("VIEW_DIST").at(0)-((180.0*g_pList.GetVectorData("IO_DIST").at(0)*g_pList.GetVectorData("VIEW_DIST").at(0))/(-disp*PI*g_pList.GetVectorData("VIEW_DIST").at(0)+180.0*g_pList.GetVectorData("IO_DIST").at(0))))
//return depth matches for OpenGL coord. Behind screen is negative and in front of screen is positive.
double GLPanel::DISPTODEPTH(double disp)
{
	//return ( m_frustum.camera2screenDist-
	//	( (180.0 * g_pList.GetVectorData("IO_DIST").at(0) * m_frustum.camera2screenDist) / 
	//	  (-disp * PI * m_frustum.camera2screenDist + 180.0 * g_pList.GetVectorData("IO_DIST").at(0))) );

	double depth;
	if (disp == 0)
		depth = 0;
	else
		depth = 
		( m_frustum.camera2screenDist-
		( (180.0 * g_pList.GetVectorData("IO_DIST").at(0) * m_frustum.camera2screenDist) / 
		  (-disp * PI * m_frustum.camera2screenDist + 180.0 * g_pList.GetVectorData("IO_DIST").at(0))) );

	// for debug
	//if (! (depth  > (-m_frustum.camera2screenDist + m_frustum.clipNear) && 
	//	depth < (-m_frustum.camera2screenDist + m_frustum.clipFar)) )
	//{
	//	depth = depth;
	//}
	return depth;
}

GLvoid GLPanel::DrawCutoutTexture()
{
	// Now, allow drawing texture in the mask where the stencil pattern is 0x1
	// and do not make any further changes to the stencil buffer
	glStencilFunc(GL_EQUAL, 0x1, 0x1);
	glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
	glEnable(GL_POLYGON_SMOOTH);

	// line number of half cutout circle
	double lineNum = floor((m_starfield.cutoutRadius-m_starfield.cutoutTextureSeparation/2.0)/m_starfield.cutoutTextureSeparation)+1.0;
	glPushMatrix();
	//Translate to cutout center
	//glTranslated(m_starfield.fixationPointLocation[0] + m_Lateral, m_starfield.fixationPointLocation[1] - m_Heave, m_starfield.fixationPointLocation[2] - m_Surge);
	glRotated(m_starfield.cutoutTextureRotAngle, 0.0, 0.0, 1.0);
	glLineWidth(m_starfield.cutoutTextureLineWidth);
	//draw the grid texture at origin.
	glBegin(GL_LINES);
		for(double i=0; i<lineNum+1.0; i++)
		{
			// horizontal lines
			glVertex3d(-m_starfield.cutoutRadius, m_starfield.cutoutTextureSeparation*(i+0.5), 0.0);
			glVertex3d(m_starfield.cutoutRadius, m_starfield.cutoutTextureSeparation*(i+0.5), 0.0);
			glVertex3d(-m_starfield.cutoutRadius, -m_starfield.cutoutTextureSeparation*(i+0.5), 0.0);
			glVertex3d(m_starfield.cutoutRadius, -m_starfield.cutoutTextureSeparation*(i+0.5), 0.0);
			// vertical lines
			glVertex3d(m_starfield.cutoutTextureSeparation*(i+0.5), -m_starfield.cutoutRadius, 0.0);
			glVertex3d(m_starfield.cutoutTextureSeparation*(i+0.5), m_starfield.cutoutRadius, 0.0);
			glVertex3d(-m_starfield.cutoutTextureSeparation*(i+0.5), -m_starfield.cutoutRadius, 0.0);
			glVertex3d(-m_starfield.cutoutTextureSeparation*(i+0.5), m_starfield.cutoutRadius, 0.0);
		}
	glEnd();
	glPopMatrix();
}

// draw eye masks
GLvoid GLPanel::DrawEyeMask(int whichEye)
{
	glEnableClientState(GL_VERTEX_ARRAY);
	glVertexPointer(3, GL_DOUBLE, 0, m_eyeMask[whichEye].quadsMaskVertex3D);
	glDrawArrays(GL_QUADS,0,m_eyeMask[whichEye].count/3);
	glDisableClientState(GL_VERTEX_ARRAY);
}

GLvoid GLPanel::DrawCutoff()
{
	// Turn off polygon smoothing otherwise we get weird lines in the
	// triangle fan.
	glDisable(GL_POLYGON_SMOOTH);

	// Use 0 for clear stencil, enable stencil test
	glClearStencil(0);
	glEnable(GL_STENCIL_TEST);

	// All drawing commands fail the stencil test, and are not
	// drawn, but increment the value in the stencil buffer.
	glStencilFunc(GL_NEVER, 0x0, 0x0);
	glStencilOp(GL_INCR, GL_INCR, GL_INCR);

	// Draw a circle.
	//glColor3d(1.0, 1.0, 1.0);
	glPushMatrix();
		if(g_pList.GetVectorData("FP_TRANSLATION").at(0)==0) // FP moving with camera
			glTranslatef(m_starfield.fixationPointLocation[0] + m_Lateral,
				m_starfield.fixationPointLocation[1] - m_Heave,
				m_starfield.fixationPointLocation[2] - m_Surge);
		else if(g_pList.GetVectorData("FP_TRANSLATION").at(0)==1) // FP stationary
			glTranslatef(m_starfield.fixationPointLocation[0],
				m_starfield.fixationPointLocation[1],
				m_starfield.fixationPointLocation[2]);

		glBegin(GL_TRIANGLE_FAN);
			//glVertex3d(m_starfield.fixationPointLocation[0] + m_Lateral,
			//		   m_starfield.fixationPointLocation[1] - m_Heave,
			//		   );
			for(double dAngle = 0; dAngle <= 360.0; dAngle += 2.0) {
				/*glVertex3d(m_starfield.cutoutRadius * cos(dAngle*DEG2RAD) + m_starfield.fixationPointLocation[0] + m_Lateral,
						m_starfield.cutoutRadius * sin(dAngle*DEG2RAD) + m_starfield.fixationPointLocation[1] - m_Heave,
						m_starfield.fixationPointLocation[2] - m_Surge);*/
				glVertex3d(m_starfield.cutoutRadius * cos(dAngle*DEG2RAD), m_starfield.cutoutRadius * sin(dAngle*DEG2RAD), 0);
			}
		glEnd();
		// Draw cutout texture with cut off mask
		if (m_starfield.useCutoutTexture == true) DrawCutoutTexture();
	glPopMatrix();

	// Now, allow drawing, except where the stencil pattern is 0x1
	// and do not make any further changes to the stencil buffer
	//glStencilFunc(GL_NOTEQUAL, 0x1, 0x1);
	glStencilFunc(GL_EQUAL, 0, 255);
	glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
	

	// Turn smoothing back on to draw the star field.
	glEnable(GL_POLYGON_SMOOTH);
}

GLvoid GLPanel::DrawStimMask()
{
	// Turn off polygon smoothing otherwise we get weird lines in the
	// triangle fan.
	glDisable(GL_POLYGON_SMOOTH);

	// Use 0 for clear stencil, enable stencil test
	if (m_starfield.useCutout == false) glClearStencil(0); //don't clear it when we use cutout
	glEnable(GL_STENCIL_TEST);

	// All drawing commands fail the stencil test, and are not
	// drawn, but increment the value in the stencil buffer.
	glStencilFunc(GL_NEVER, 0x0, 0x0);
	glStencilOp(GL_INCR, GL_INCR, GL_INCR);

	// Draw a circle.
	glColor3d(0.0, 0.0, 0.0);
	GLUquadricObj* mask = gluNewQuadric();
	glPushMatrix();
		//don't move the mask with camera
		glTranslatef(m_starfield.fixationPointLocation[0] + m_Lateral,
					m_starfield.fixationPointLocation[1] - m_Heave,
					m_starfield.fixationPointLocation[2] - m_Surge);
		glTranslated( stimMaskVars.at(0), stimMaskVars.at(1), 0.0);
		gluDisk(mask, 0.0, stimMaskVars.at(2), 100, 1);
	glPopMatrix();

	// Now, allow drawing at stencil pattern is 0x1
	// and do not make any further changes to the stencil buffer
	glStencilFunc(GL_EQUAL, 1, 255);
	glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);

	
	
	// Turn smoothing back on to draw the star field.
	glEnable(GL_POLYGON_SMOOTH);

}


GLvoid GLPanel::DrawCross(double x, double y, double z, double lineLength, double lineWidth, int whichEye)
{
#if USE_MATLAB
	GLint viewport[4];
	GLdouble modelview[16];
	GLdouble projection[16];
	GLdouble winX, winY, winZ;

	glGetDoublev( GL_MODELVIEW_MATRIX, modelview );
	glGetDoublev( GL_PROJECTION_MATRIX, projection );
	glGetIntegerv( GL_VIEWPORT, viewport );

	int ans = gluProject(x, y, z, modelview, projection, viewport, &winX, &winY, &winZ);
	winPointTraj[whichEye].X.push_back(winX);
	winPointTraj[whichEye].Y.push_back(winY);
#endif

	glPushMatrix();
		glTranslated(x,y,z);
		glLineWidth(lineWidth);
		//glColor3d(1.0, 0.0, 0.0);
		glBegin(GL_LINES);
		// horizontal line
		if(calibTranOn[0]== 1){
			glVertex3d(lineLength/2, 0.0, 0.0);
			glVertex3d(-lineLength/2, 0.0, 0.0);
		}
		// vertical line
		if(calibTranOn[1]== 1){
			glVertex3d(0.0, lineLength/2, 0.0);
			glVertex3d(0.0, -lineLength/2, 0.0);
		}
		glEnd();
	glPopMatrix();
}

Point3 GLPanel::FindPointByTwoPointForm(Point3 a, Point3 b, double depthZ)
{
	Point3 c;
	double z_ratio = (depthZ-b.z)/(a.z-b.z);
	c.x = b.x+(a.x-b.x)*z_ratio; 
	c.y = b.y+(a.y-b.y)*z_ratio;
	c.z = depthZ;

	return c;
}

GLvoid GLPanel::GenerateWorldRefObj()
{
	double ratio = g_pList.GetVectorData("W_REF_RATIO").at(0);
	Point3 volume(	g_pList.GetVectorData("W_REF_VOLUME").at(0),
					g_pList.GetVectorData("W_REF_VOLUME").at(1),
					g_pList.GetVectorData("W_REF_VOLUME").at(2));
	Point3 origin(	g_pList.GetVectorData("W_REF_ORIGIN").at(0),
					g_pList.GetVectorData("W_REF_ORIGIN").at(1),
					g_pList.GetVectorData("W_REF_ORIGIN").at(2));


	Point3 cornerLinePoints[6]; //one corner of world frame need 3 lines and 6 points
	Point3 lineLength = volume*ratio/2.0; //line length in x,y,z direction.
	int pn=0; //point number for worldRefVertex3D array

	//build 8 corner lines
	for(double xi=-1.0; xi<=1.0; xi+=2.0)
		for(double yi=-1.0; yi<=1.0; yi+=2.0)
			for(double zi=-1.0; zi<=1.0; zi+=2.0)
			{	//set corner point first
				cornerLinePoints[0].set(xi*volume.x/2.0, yi*volume.y/2.0, zi*volume.z/2.0);
				for(int i=1; i<6; i++) cornerLinePoints[i].set(cornerLinePoints[0]);
				//adjust the other ending line point
				if(cornerLinePoints[1].x > 0.0) cornerLinePoints[1].x -= lineLength.x; 
				else cornerLinePoints[1].x += lineLength.x;
				if(cornerLinePoints[3].y > 0.0) cornerLinePoints[3].y -= lineLength.y;
				else cornerLinePoints[3].y += lineLength.y;
				if(cornerLinePoints[5].z > 0.0) cornerLinePoints[5].z -= lineLength.z;
				else cornerLinePoints[5].z += lineLength.z;
				//store points in worldRefVertex3D array and shifted by origin
				for(int i=0; i<6; i++)
				{
					worldRefVertex3D[pn++] = cornerLinePoints[i].x+origin.x;
					worldRefVertex3D[pn++] = cornerLinePoints[i].y+origin.y;
					worldRefVertex3D[pn++] = cornerLinePoints[i].z+origin.z;
				}
			}

}

GLvoid GLPanel::DrawWorldRefObj(int whichEye)
{
	if(g_pList.GetVectorData("W_REF_RATIO").at(0) > 0.0)
	{
		if (whichEye == LEFT_EYE) 
			glColor3d(	g_pList.GetVectorData("W_REF_LEYE_COLOR").at(0),
						g_pList.GetVectorData("W_REF_LEYE_COLOR").at(1),
						g_pList.GetVectorData("W_REF_LEYE_COLOR").at(2));
		else
			glColor3d(	g_pList.GetVectorData("W_REF_REYE_COLOR").at(0),
						g_pList.GetVectorData("W_REF_REYE_COLOR").at(1),
						g_pList.GetVectorData("W_REF_REYE_COLOR").at(2));

		glLineWidth(g_pList.GetVectorData("W_REF_LINE_WIDTH").at(0));
		glEnableClientState(GL_VERTEX_ARRAY);
		glVertexPointer(3, GL_FLOAT, 0, worldRefVertex3D);
		glDrawArrays(GL_LINES,0,8*3*2);//8 corners * 3 lines * 2 points
		glDisableClientState(GL_VERTEX_ARRAY);
	}
}
