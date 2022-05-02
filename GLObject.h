#pragma once

#include "GlobalDefs.h"

class GLPanel;

// Represents a point in 3D.
typedef struct XYZ3
{
	GLdouble x, y, z;
} XYZ3;

// Defines a object 3D field of stars.
typedef struct OBJ_STARFIELD_STRUCT
{
	vector<double>	origin,			// (x,y,z) in meter. All objects draw at (0,0,0) location and add offset of origin, so origin is in meter and uses for MoogDotsCom easily. 
					dimensions,		// Width, height, depth dimensions of starfield.
					triangle_size,	// Base width, height for each star.
					starLeftColor,	// (r,g,b) value of left eye starfield.
					starRightColor,	// (r,g,b) value of right eye starfield.
					starPattern,	// Number of segments: cuboid (nW, nH, nD) / ellipsoid (nR, nAzi, nEle)
					textureSegment;	// Number of segments on Azi and Ele
	double density,					// how many stars per cubic cm
		   enable,					// On=1 or Off=0
		   luminance,				// multiple on RGB color
		   luminanceFlag,			// On=1 or Off=0 for changing luminance
		   probability,				// coherence probablility
		   use_lifetime;			// On=1 or Off=0
	int lifetime,					// frame numbers
		shape,						// 0=ellipsoid, 1=cuboid
		dotSize,					// pixel
		starType,					// 0=point, 1=triangles
		starDistrubute,				// 0=random, 1=pattern
		bodyType,					// 0=soild, 1=hollow
		densityType,				// 0=stars/cm^3, 1=deg, 2=total number of stars
		numOfCopy;					// number of copies of the object to draw on screen
	double	initRotAngle ,			// Object initial rotation angle in deg.
			rotSpeed,				// deg/s
			rotAzi,					// rotation azimuth in deg
			rotEle;					// rotation elevation in deg
	vector<XYZ3> copyObjTrans;		// copy objects translation distance.
} ObjStarField;


class GLObject
{
private:
	ObjStarField m_objfield;		// Defines the object star field which will be rendered.
	GLPanel *m_glPanel;				// parent's point.
	GLfloat *m_objectFieldVertex3D;	// for glDrawArrays() to speed up the drawing
	XYZ3 m_objFieldTran;			// translation of object.

	int m_totalStars;									// number of stars according to density
	GLuint m_objTextureL, m_objTextureR,				// object texture for Left and Right eye
		   m_textureWidthPixel, m_textureHeightPixel;	// in pixel - we will use glCopyTexSubImage2D() of H & W on the screen
	// This is in world coord. 
	// We draw a small rectangular of star field in front of ellipsoid (x,y,z)=(0,0,c). 
	// Then it will project on the screen with a big rectangular 
	// such that the size is correct for texture map on the ellipsoid.
	double	m_textureWidthCM, m_textureHeightCM,		// small rectangular
			m_textureProjWidthCM, m_textureProjHeightCM,// projection large rectangular
			m_rectShiftX,								// rectangular shift according to left or right frustrum 
			m_rotAngle,									// according to object rotation speed
			m_dynLumRatio;								// dynamic luminance ratio
	XYZ3	m_rotVector;								// unit vector for object rotation.

	// Calculate object texture width and height
	GLvoid CalculateObjectTexture();
	// Add object star ( point or trangle) in objectFieldVertex3D for glDrawArrays()
	int AddObjStar(double starType, double baseX, double baseY, double baseZ, double ts0, double ts1, int j);
	// Copy specific area from screen to make the texture
	GLvoid GenerateObjectTexture(int whichEye);
	// Find the distance of two points on ellipsoid
	double Ellipsoid2PointDist(double a, double b, double c, XYZ3 center, double azi1, double ele1, double azi2, double ele2, bool degree);
	// Create An Empty Texture 
	GLuint EmptyTexture(int w, int h);
	// Draws the generated object starfield.
	GLvoid DrawObjectStarField();
	// Delete object texture
	GLvoid DeleteObjTexture();
	// Generates the object starfield.
	GLvoid GenerateObjectStarField();
	// Draw the texture mapping ellipsoid
	GLvoid DrawTextureEllipsoid(int whichEye, double ax, double by, double cz, int nAzi, int nEle, double theta1, double theta2, double phi1, double phi2);
	// Draw the texture mapping cuboid
	GLvoid DrawTextureCuboid(int whichEye, double width, double height, double depth);
	// Draw all different kind of objects at assign position. 
	GLvoid DrawObject(int whichEye);

public:
	GLObject(GLPanel *glPanel, ObjStarField objfield);
	~GLObject(void);

	// Setup all parameters for object
	void SetupObject(ObjStarField objField, bool calStarField=true);
	// Get object star field parameters(const for read-only m_objfield)
	ObjStarField GetObject() const {return m_objfield;};
	// Used to alter star locations due to their lifetimes.
	GLvoid ModifyObjectStarField();
	// Rendering Object depends on which eye
	GLvoid RenderObj(int whichEye);
	// Set object translate location
	void SetObjFieldTran(double x, double y, double z);
	// Get object translate location
	XYZ3 GetObjFieldTran(){return m_objFieldTran;};
	// Set object rotation angle
	void SetObjRotAngle(double rotAngle);
	// Create object texture
	GLvoid CreateObjectTexture();
	// Get object origin
	XYZ3 GetObjOrigin();
	// Set object enable
	GLvoid SetObjEnable(double enable){m_objfield.enable = enable;};
	// Directly setup object's rotation vector
	GLvoid SetObjRotVector(double x, double y, double z);
	// Set dynamic luminance
	GLvoid SetDynLumRatio(double dynLumRatio){m_dynLumRatio = dynLumRatio;};
};
