#include "StdAfx.h"
#include "GLObject.h"
#include "GLWindow.h"

GLObject::GLObject(GLPanel *glPanel, ObjStarField objfield)
{
	m_glPanel = glPanel;
	m_objFieldTran.x = m_objFieldTran.y = m_objFieldTran.z = 0.0;
	m_textureWidthCM = 20;
	m_textureHeightCM = 10;
	m_textureProjWidthCM = 20;
	m_textureProjHeightCM = 10;
	m_textureWidthPixel = 200;
	m_textureHeightPixel = 100;
	m_objTextureL = m_objTextureR = 0;
	m_rotAngle = 0.0;
	m_objectFieldVertex3D = NULL;
	m_totalStars = 0;									
	m_rectShiftX = 0;
	m_rotAngle = 0;
	m_rotVector.x = m_rotVector.y = m_rotVector.z = 0;
	m_dynLumRatio = 1.0;

	SetupObject(objfield);
}

GLObject::~GLObject(void)
{
	DeleteObjTexture();

	if(m_objectFieldVertex3D != NULL){
		delete [] m_objectFieldVertex3D;
	}
}

void GLObject::SetupObject(ObjStarField objfield, bool calStarField)
{
	m_objfield = objfield;

	if(calStarField) GenerateObjectStarField();
}

GLvoid GLObject::SetObjRotVector(double x, double y, double z)
{
	m_rotVector.x=x; 
	m_rotVector.y=y; 
	m_rotVector.z=z;
};

GLvoid GLObject::GenerateObjectStarField()
{
	//This function is only called when DO_MOVEMENT == -1.0;
	//We are initial the rotation angle and vector once for object;
	//Then allow to change in main program.
	m_rotAngle = 0.0;
	SetObjRotVector(cos(m_objfield.rotEle*DEG2RAD)*cos(m_objfield.rotAzi*DEG2RAD), //x
					sin(m_objfield.rotEle*DEG2RAD), //y
					cos(m_objfield.rotEle*DEG2RAD)*sin(m_objfield.rotAzi*DEG2RAD)); // z

	int i;
	double baseX, baseY, baseZ,
		   sd0, sd1, sd2,
		   ts0, ts1;
	int n0, n1, n2;
	XYZ3 e, r;

	n0 = (int)m_objfield.starPattern.at(0); // number on separate of radius or width
	n1 = (int)m_objfield.starPattern.at(1); // number on separate of Azimuth or height
	n2 = (int)m_objfield.starPattern.at(2); // number on separate of Elevation or deepth

	if(m_objfield.starDistrubute == 1.0){	// distrubution in pattern
		if (n0 <= 0 || n1 <= 0 || n2 <= 0) return; // error setting
	}
	
	// Grab the object starfield dimensions and triangle size.  Pulling this stuff out
	// of the vectors now produces a 20 fold increase in speed in the following
	// loop.
	sd0 = m_objfield.dimensions[0];
	sd1 = m_objfield.dimensions[1];
	sd2 = m_objfield.dimensions[2];
	ts0 = m_objfield.triangle_size[0];
	ts1 = m_objfield.triangle_size[1];

	// Delete the old Object Star array if needed.
	if(m_objectFieldVertex3D != NULL){
		delete [] m_objectFieldVertex3D;
	}

	// Determine the total number of stars needed to create an average density determined
	// from the StarField structure.
	if(m_objfield.starDistrubute == 0.0){	// random distrubution
		if(m_objfield.densityType == 2.0) m_totalStars = m_objfield.density;
		else
		{
			if(m_objfield.bodyType == 0.0){	// solid body
				if(m_objfield.shape == 0.0) // Ellipsoid: 4/3 * pi * abc * density
				{
					if(sd2==0.0) m_totalStars = int(PI*sd0*sd1*m_objfield.density); //flat ellipse:pi*a*b*density
					else m_totalStars = int(4.0/3.0*PI*sd0*sd1*sd2/8.0*m_objfield.density);
				}
				else if(m_objfield.shape == 1.0) // Cuboid: W*H*D*density
				{
					if(sd2==0.0) m_totalStars = int(sd0*sd1*m_objfield.density); // flat square
					else m_totalStars = int(sd0*sd1*sd2*m_objfield.density);
				}
			}
			else if(m_objfield.bodyType == 1.0 || m_objfield.bodyType == 2.0){	// hollow body (surface area) or texture mapping
				if(m_objfield.shape == 0.0){ // Ellipsoid area: Knud Thomsen's formula 
					double p = 1.6075; // a value of p = 8/5 = 1.6 is optimal for nearly spherical ellipsoids
					double area = 4*PI*pow((pow(sd0/2.0,p)*pow(sd1/2.0,p)+pow(sd0/2.0,p)*pow(sd2/2.0,p)+pow(sd1/2.0,p)*pow(sd2/2.0,p))/3.0,1.0/p);
					m_totalStars = int(area*m_objfield.density);
				}
				else if(m_objfield.shape == 1.0) // Cuboid: 2*(W*H+H*D+W*D)*density
					m_totalStars = int(2*(sd0*sd1+sd1*sd2+sd0*sd2)*m_objfield.density);
			}
		}
	}
	else if(m_objfield.starDistrubute == 1.0){	// distrubution in pattern
		if(m_objfield.shape == 0.0){ // Ellipsoid
			if(m_objfield.bodyType == 0.0)	// solid body
				m_totalStars = n0*(2 + (n2-1)*n1) + 1; // 2->north and south pole star; 1->center star;
			else if(m_objfield.bodyType == 1.0)	// hollow body (draw surface only)
				m_totalStars = 2 + (n2-1)*n1; // 2->north and south pole star; no center star;
			else if(m_objfield.bodyType == 2.0)	// texture mapping
				m_totalStars = (n1+1)*(n2+1);
		}
		else if(m_objfield.shape == 1.0){ // Cuboid
			if(m_objfield.bodyType == 0.0)	// solid body
				m_totalStars = int((n0+1)*(n1+1)*(n2+1));
			else if(m_objfield.bodyType == 1.0)	// hollow body (draw surface only)
				m_totalStars = int(2*(n1-1)*(n2-1)+2*(n0+1)*(n2-1)+2*(n0+1)*(n1+1));
			else if(m_objfield.bodyType == 2.0)	// texture mapping
				m_totalStars = int(2*(n1+1)*n2+2*(n0+1)*n2+(n0+1)*(n1+1));
		}
	}

	if(m_objfield.bodyType == 2.0){	// texture mapping
			CalculateObjectTexture();
	}

	// Allocate the object field array (triangle has 3 points and each point has xyz coord).
	if(m_objfield.starType == 0.0) // draw point
		m_objectFieldVertex3D = new GLfloat[m_totalStars*3];
	else if(m_objfield.starType == 1.0) // draw triangle
		m_objectFieldVertex3D = new GLfloat[m_totalStars*3*3];

	int face = 0;
	int j = 0;
	if(m_objfield.starDistrubute == 0.0){	// random distrubution
		for (i = 0; i < m_totalStars; i++) {
			if(m_objfield.bodyType == 0.0){	// solid body
				// Find a random point to base our triangle around.
				baseX = (double)rand()/(double)RAND_MAX*sd0 - sd0/2.0;
				baseY = (double)rand()/(double)RAND_MAX*sd1 - sd1/2.0;
				baseZ = (double)rand()/(double)RAND_MAX*sd2 - sd2/2.0; //For 2D flat square or ellipse, baseZ = 0 automatically.

				if(sd2==0)
				{
					if(m_objfield.shape == 0.0 && // flat ellipse: x^2/a^2+y^2/b^2=1
						4.0*baseX*baseX/sd0/sd0+4.0*baseY*baseY/sd1/sd1 > 1){
							i--;		// out of range, try again.
							continue;
					}
				}
				else
				{
					if(m_objfield.shape == 0.0 && // Ellipsoid: x^2/a^2+y^2/b^2+z^2/c^2=1
					4.0*baseX*baseX/sd0/sd0+4.0*baseY*baseY/sd1/sd1+4.0*baseZ*baseZ/sd2/sd2 > 1){
						i--;		// out of range, try again.
						continue;
					}
				}
			}
			else if(m_objfield.bodyType == 1.0){	// hollow body (surface area)
				if(m_objfield.shape == 0.0){ // Ellipsoid
					// Find a random point on hollow sphere (Yes, don't know how to evenly distribute star on ellipsoid)
					double ele = asin(((double)rand()/(double)RAND_MAX)*2-1);	//-90(-1) <= elevation <= 90(1); The Lambert Projection
					double azi = ((double)rand()/(double)RAND_MAX)*2*PI;		//0 <= azimth <= 360
					e.x = cos(ele) * cos(azi);
					e.y = sin(ele);
					e.z = cos(ele) * sin(azi);
					baseX = sd0/2.0 * e.x;
					baseY = sd1/2.0 * e.y;
					baseZ = sd2/2.0 * e.z;
				}
				else if(m_objfield.shape == 1.0){ // Cuboid
					// Find a random point to base our triangle around.
					baseX = (double)rand()/(double)RAND_MAX*sd0 - sd0/2.0;
					baseY = (double)rand()/(double)RAND_MAX*sd1 - sd1/2.0;
					baseZ = (double)rand()/(double)RAND_MAX*sd2 - sd2/2.0;

					face = rand()%6;
					switch ( face )
					{
						case 0: // front face
							baseZ = sd2/2.0;
							break;
						case 1: // back face
							baseZ = -sd2/2.0;
							break;
						case 2: // left face
							baseX = sd0/2.0;
							break;
						case 3: // right face
							baseX = -sd0/2.0;
							break;
						case 4: // top face
							baseY = sd1/2.0;
							break;
						case 5: // bottom face
							baseY = -sd1/2.0;
							break;
					}
				}
			}
			else if(m_objfield.bodyType == 2.0){	// texture mapping
				if(m_objfield.shape == 0.0){ // Ellipsoid
					// Find a random point to base our triangle around.
					double ele = asin(((double)rand()/(double)RAND_MAX)*2-1);	//-90(-1) <= elevation <= 90(1); The Lambert Projection
					baseX = (double)rand()/(double)RAND_MAX*m_textureWidthCM - m_textureWidthCM/2.0;
					baseY = (m_textureHeightCM/2.0) * ele/(PI/2.0);
					baseZ = sd2/2.0;
				}
				else if(m_objfield.shape == 1.0){ //cuboid
					// Find a random point to base our triangle around.
					baseX = (double)rand()/(double)RAND_MAX*m_textureWidthCM - m_textureWidthCM/2.0;
					baseY = (double)rand()/(double)RAND_MAX*m_textureHeightCM - m_textureHeightCM/2.0;
					baseZ = sd2/2.0;
				}
			}

			j = AddObjStar(m_objfield.starType, baseX, baseY, baseZ, ts0, ts1, j);
		}
	}
	else if(m_objfield.starDistrubute == 1.0){	// distrubution in pattern
		if(m_objfield.shape == 0.0){ // Ellipsoid
			if(m_objfield.bodyType == 0.0 || m_objfield.bodyType == 1.0){ // solid or hollow body type
				r.x = sd0/2.0;
				r.y = sd1/2.0;
				r.z = sd2/2.0;
				int in0=0;
				while(in0 < n0){// number on separate of radius
					double ele=-90+180/n2;
					for(int in2=1; in2<n2; in2++){ // number on separate of Elevation
						double azi=0;
						for(int in1=0; in1<n1; in1++){ // number on separate of Azimuth
							e.x = cos(ele*DEG2RAD) * cos(azi*DEG2RAD);
							e.y = sin(ele*DEG2RAD);
							e.z = cos(ele*DEG2RAD) * sin(azi*DEG2RAD);
							baseX = r.x * e.x;
							baseY = r.y * e.y;
							baseZ = r.z * e.z;
							j = AddObjStar(m_objfield.starType, baseX, baseY, baseZ, ts0, ts1, j);
							azi+=360/n1;
						}
						ele+=180/n2;
					}

					//Add north and south pole star
					j = AddObjStar(m_objfield.starType, 0.0, r.y, 0.0, ts0, ts1, j);
					j = AddObjStar(m_objfield.starType, 0.0, -r.y, 0.0, ts0, ts1, j);

					if(m_objfield.bodyType == 1.0) break;	// hollow body (draw surface only)

					r.x -= sd0/2.0/n0;	// number on separate of radius
					r.y -= sd1/2.0/n0;
					r.z -= sd2/2.0/n0;
					in0++;
				}

				// Add center star
				if(m_objfield.bodyType == 0.0){	// solid body
					j = AddObjStar(m_objfield.starType, 0.0, 0.0, 0.0, ts0, ts1, j);
				}
			}
			else if(m_objfield.bodyType == 2.0){	// texture mapping
				double h=-m_textureHeightCM/2.0;
				for(int ih=0; ih<=n2; ih++){// number on separate of Elevation
					double w=-m_textureWidthCM/2.0;
					for(int iw=0; iw<=n1; iw++){// number on separate of Azimuth
						j = AddObjStar(m_objfield.starType, w, h, sd2/2.0, ts0, ts1, j);
						w+=m_textureWidthCM/n1;
					}
					h+=m_textureHeightCM/n2;
				}
			}
		}
		else if(m_objfield.shape == 1.0){ // Cuboid
			if(m_objfield.bodyType == 0.0 || m_objfield.bodyType == 1.0){ // solid or hollow body type
				baseZ=sd2/2.0;
				for(int in2=0; in2<=n2; in2++){
					baseY=sd1/2.0;
					for(int in1=0; in1<=n1; in1++){
						baseX=sd0/2.0;
						for(int in0=0; in0<=n0; in0++){
							bool skip = ( m_objfield.bodyType == 1.0 && // hollow body (draw surface only)
											(baseX<sd0/2.0 && baseX>-sd0/2.0) && (baseY<sd1/2.0 && baseY>-sd1/2.0) && (baseZ<sd2/2.0 && baseZ>-sd2/2.0) );
							if(!skip) j = AddObjStar(m_objfield.starType, baseX, baseY, baseZ, ts0, ts1, j);
							baseX-=sd0/n0;
						}
						baseY-=sd1/n1;
					}
					baseZ-=sd2/n2;
				}
			}
			else if(m_objfield.bodyType == 2.0){	// texture mapping
				// This is in world coord. 
				// We draw a small rectangular of star field in front of cubsoid at (x,y,z)=(0,0,D/2). 
				// Then it will project on the screen with a big rectangular 
				// such that the size is fit for texture map on the cubsoid.
				sd0 = sd0*m_textureWidthCM/m_textureProjWidthCM;
				sd1 = sd1*m_textureHeightCM/m_textureProjHeightCM;
				sd2 = sd2*m_textureWidthCM/m_textureProjWidthCM;

				double h=-sd1/2.0;
				for(int ih=0; ih<=n1; ih++){
					double d=sd0/2.0+sd2/n2;
					for(int id=1; id<=n2; id++){
						j = AddObjStar(m_objfield.starType, d, h, sd2/2.0, ts0, ts1, j);
						j = AddObjStar(m_objfield.starType, -d, h, sd2/2.0, ts0, ts1, j);
						d+=sd2/n2;
					}

					double w=-sd0/2.0;
					for(int iw=0; iw<=n0; iw++){
						j = AddObjStar(m_objfield.starType, w, h, sd2/2.0, ts0, ts1, j);
						w+=sd0/n0;
					}

					h+=sd1/n1;
				}

				double w=-sd0/2.0;
				for(int iw=0; iw<=n0; iw++){
					double d=sd1/2.0+sd2/n2;
					for(int id=1; id<=n2; id++){
						j = AddObjStar(m_objfield.starType, w, d, sd2/2.0, ts0, ts1, j);
						j = AddObjStar(m_objfield.starType, w, -d, sd2/2.0, ts0, ts1, j);
						d+=sd2/n2;
					}
					w+=sd0/n0;
				}
			}
		}
	}

	if(m_objfield.bodyType == 2.0) CreateObjectTexture();	// texture mapping
}

GLvoid GLObject::CreateObjectTexture()
{
	CalculateObjectTexture();
	GenerateObjectTexture(LEFT_EYE);
	GenerateObjectTexture(RIGHT_EYE);
}

GLvoid GLObject::CalculateObjectTexture()
{
	double sd0, sd1, sd2;
	sd0 = m_objfield.dimensions[0];
	sd1 = m_objfield.dimensions[1];
	sd2 = m_objfield.dimensions[2];

	// get glWindow width and height in pixel
	int w = m_glPanel->GetScreenWidth();
	int h = m_glPanel->GetScreenHeigth();	
	//GetClientSize(&w, &h);

	XYZ3 c;
	c.x = c.y = c.z = 0.0;
	double  tw = 0, th = 0;
	if(m_objfield.shape == 0.0){ // Ellipsoid
		// Find biggest circumference of ellipsoid at azimuth
		for(double azi=0.0; azi<360.0; azi+=360.0/m_objfield.textureSegment.at(0)){
			tw += Ellipsoid2PointDist(sd0/2.0, sd1/2.0, sd2/2.0, c, azi, 0, azi+360.0/m_objfield.textureSegment.at(0), 0, true);
		}

		// Find half biggest circumference of ellipsoid at elevation
		for(double ele=-90.0; ele<90.0; ele+=180.0/m_objfield.textureSegment.at(1)){
			th += Ellipsoid2PointDist(sd0/2.0, sd1/2.0, sd2/2.0, c, 90, ele, 90, ele+180.0/m_objfield.textureSegment.at(1), true);
		}
	}
	else if (m_objfield.shape == 1.0){ // Cuboid
		// Find area of texture that can build 5 face of cuboid (except back face)
		tw = sd0+2.0*sd2; 
		th = sd1+2.0*sd2;
		if(m_objfield.starDistrubute == 0.0)	// random distrubution
			m_totalStars = int(tw*th*m_objfield.density);
	}

	Frustum* frustum = m_glPanel->GetFrustum();
	m_textureWidthPixel = w*tw/frustum->screenWidth;
	m_textureHeightPixel = h*th/frustum->screenHeight;

	if (m_objTextureL != 0) glDeleteTextures(1,&m_objTextureL);
	if (m_objTextureR != 0) glDeleteTextures(1,&m_objTextureR);
	m_objTextureL = EmptyTexture(m_textureWidthPixel, m_textureHeightPixel);
	m_objTextureR = EmptyTexture(m_textureWidthPixel, m_textureHeightPixel);

	// This is in world coord. 
	// We draw a small rectangular of star field in front of ellipsoid at (x,y,z)=(0,0,+c). 
	// Then it will project on the screen with a big rectangular 
	// such that the size is fit for texture map on the ellipsoid.
	//double eye2center = frustum->eyeSeparation/2.0;
	double camera2ellipsoid = frustum->camera2screenDist-m_objfield.dimensions.at(2)/2.0;
	//if(eye2center != 0)	m_textureWidthCM = eye2center*tw/(m_frustum.camera2screenDist*eye2center/camera2ellipsoid);
	//else m_textureWidthCM = tw*camera2ellipsoid/m_frustum.camera2screenDist;
	
	// Eye is at center.
	m_textureWidthCM = tw*camera2ellipsoid/frustum->camera2screenDist;
	m_textureHeightCM = th*camera2ellipsoid/frustum->camera2screenDist;
	

	m_textureProjWidthCM = tw;
	m_textureProjHeightCM = th;
	//m_rectShiftX = m_frustum.camera2screenDist*eye2center/camera2ellipsoid - eye2center;
	m_rectShiftX = 0;
}

int GLObject::AddObjStar(double starType, double baseX, double baseY, double baseZ, double ts0, double ts1, int j)
{
	if(starType == 0.0){ // draw point
		// Vertex 1
		m_objectFieldVertex3D[j++] = baseX;
		m_objectFieldVertex3D[j++] = baseY;
		m_objectFieldVertex3D[j++] = baseZ;
	}
	else if(starType == 1.0){ // draw triangle
		// Vertex 1
		m_objectFieldVertex3D[j++] = baseX - ts0/2.0;
		m_objectFieldVertex3D[j++] = baseY - ts1/2.0;
		m_objectFieldVertex3D[j++] = baseZ;

		// Vertex 2
		m_objectFieldVertex3D[j++] = baseX;
		m_objectFieldVertex3D[j++] = baseY + ts1/2.0;
		m_objectFieldVertex3D[j++] = baseZ;

		// Vertex 3
		m_objectFieldVertex3D[j++] = baseX + ts0/2.0;
		m_objectFieldVertex3D[j++] = baseY - ts1/2.0;
		m_objectFieldVertex3D[j++] = baseZ;
	}

	return j;
}

GLvoid GLObject::GenerateObjectTexture(int whichEye)
{
	double x, y;
	double eyePolarity = 1.0;

	if (whichEye == LEFT_EYE) {
#if USE_STEREO
		glDrawBuffer(GL_BACK_LEFT);
#else
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
#endif
		eyePolarity = -1.0;
	}
	else {
#if USE_STEREO
		glDrawBuffer(GL_BACK_RIGHT);
#else
		glColorMask(GL_FALSE, GL_TRUE, GL_FALSE, GL_FALSE);
#endif
	}
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);		// Clears the current scene.

	// Setup the projection matrix.
	// Always draw the starfield in center for texture.
	Frustum* frustum = m_glPanel->GetFrustum();
	m_glPanel->CalculateStereoFrustum(frustum->screenWidth, frustum->screenHeight, frustum->camera2screenDist,
						   frustum->clipNear, frustum->clipFar, 0.0, //eyePolarity*m_frustum.eyeSeparation/2.0,
						   frustum->worldOffsetX, frustum->worldOffsetZ);

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	// Setup the camera. - We always set camera at middle
	gluLookAt(0.0f, 0.0f, frustum->camera2screenDist,		// Camera origin
			  0.0f, 0.0f, frustum->camera2screenDist-1.0f,	// Camera direction
			  0.0, 1.0, 0.0); // Which way is up

	// Draw the left starfield.
	if(m_objfield.starType == 0.0) glPointSize(m_objfield.dotSize);
	if (whichEye == LEFT_EYE) {
		glColor3d(m_objfield.starLeftColor[0] * m_objfield.luminance,		// Red
				m_objfield.starLeftColor[1] * m_objfield.luminance,		// Green
				m_objfield.starLeftColor[2] * m_objfield.luminance);	// Blue
	}
	else{
		glColor3d(m_objfield.starRightColor[0] * m_objfield.luminance,		// Red
				m_objfield.starRightColor[1] * m_objfield.luminance,		// Green
				m_objfield.starRightColor[2] * m_objfield.luminance);		// Blue
	}
	DrawObjectStarField();

	//glColor3d(0.0,0.0,0.0);
	glDisable(GL_BLEND);
	glEnable(GL_TEXTURE_2D);
	if (whichEye == LEFT_EYE){
		glReadBuffer(GL_BACK_LEFT); //set current buffer
		glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_DECAL);
		// using objTextureL texture object
		glBindTexture(GL_TEXTURE_2D,m_objTextureL); 
	}
	else{
		glReadBuffer(GL_BACK_RIGHT); //set current buffer
		glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_DECAL);
		// using objTextureR texture object
		glBindTexture(GL_TEXTURE_2D,m_objTextureR);
	}

	// get glWindow width and height in pixel
	int w = m_glPanel->GetScreenWidth();
	int h = m_glPanel->GetScreenHeigth();	
	//GetClientSize(&w, &h);

	// read a rectangle of pixels from the framebuffer and uses it for a new texture.
	// find world crood of rectangle left bottom conner (x,y)
	x = -m_textureProjWidthCM/2.0 - eyePolarity*m_rectShiftX + frustum->worldOffsetX;
	y = -m_textureProjHeightCM/2.0 + frustum->worldOffsetZ;
	// change to pixel value
	x = w*(x + frustum->screenWidth/2.0)/frustum->screenWidth;
	y = h*(y + frustum->screenHeight/2.0)/frustum->screenHeight;
	glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, int(x), int(y), m_textureWidthPixel, m_textureHeightPixel);

	glEnable(GL_BLEND);
	glDisable(GL_TEXTURE_2D);
}

double GLObject::Ellipsoid2PointDist(double a, double b, double c, XYZ3 center, double azi1, double ele1, double azi2, double ele2, bool degree)
{
	XYZ3 e, p1, p2;

	if (degree){
		azi1 = azi1*DEG2RAD;
		azi2 = azi2*DEG2RAD;
		ele1 = ele1*DEG2RAD;
		ele2 = ele2*DEG2RAD;
	}

	e.x = cos(ele1) * cos(azi1);
	e.y = sin(ele1);
	e.z = cos(ele1) * sin(azi1);
	p1.x = center.x + a * e.x;
	p1.y = center.y + b * e.y;
	p1.z = center.z + c * e.z;

	e.x = cos(ele2) * cos(azi2);
	e.y = sin(ele2);
	e.z = cos(ele2) * sin(azi2);
	p2.x = center.x + a * e.x;
	p2.y = center.y + b * e.y;
	p2.z = center.z + c * e.z;

	return sqrt((p1.x-p2.x)*(p1.x-p2.x)+(p1.y-p2.y)*(p1.y-p2.y)+(p1.z-p2.z)*(p1.z-p2.z));
}

// Create An Empty Texture 
GLuint GLObject::EmptyTexture(int w, int h)
{
	GLuint txtnumber;											// Texture ID
	unsigned int* data;											// Stored Data

	// Create Storage Space For Texture Data 
	data = (unsigned int*)new GLuint[((w * h)* 4 * sizeof(unsigned int))];
	ZeroMemory(data,((w * h)* 4 * sizeof(unsigned int)));	// Clear Storage Memory

	glGenTextures(1, &txtnumber);								// Create 1 Texture
	glBindTexture(GL_TEXTURE_2D, txtnumber);					// Bind The Texture
	glTexImage2D(GL_TEXTURE_2D, 0, 4, w, h, 0,
		GL_RGBA, GL_UNSIGNED_BYTE, data);						// Build Texture Using Information In data
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
	/*
	if(m_objfield.shape == 1.0){ //cuboid
		glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
	}
	*/
	// The border is always ignored. 
	// Texels at or near the edge of the texture are used for texturing calculations, but not the border.
	//glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
	//glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);

	delete [] data;												// Release data

	return txtnumber;											// Return The Texture ID
}

GLvoid GLObject::DrawObjectStarField()
{
	glEnableClientState(GL_VERTEX_ARRAY);
	glVertexPointer(3, GL_FLOAT, 0, m_objectFieldVertex3D);
	if(m_objfield.starType == 0.0) // draw point
		glDrawArrays(GL_POINTS,0,m_totalStars);
	else if(m_objfield.starType == 1.0) // draw triangle
		glDrawArrays(GL_TRIANGLES,0,m_totalStars*3);
	glDisableClientState(GL_VERTEX_ARRAY);
}

GLvoid GLObject::DeleteObjTexture()
{
	if (m_objTextureL != 0) glDeleteTextures(1,&m_objTextureL);
	if (m_objTextureR != 0) glDeleteTextures(1,&m_objTextureR);
	m_objTextureL = m_objTextureR = 0;
}

GLvoid GLObject::ModifyObjectStarField()
{
	int i;
	double baseX, baseY, baseZ, prob,
		   sd0, sd1, sd2,
		   ts0, ts1;
	XYZ3 e;

	if(m_objfield.starDistrubute != 0.0) return;	// not random distrubution

	// Grab the object starfield dimensions and triangle size.  Pulling this stuff out
	// of the vectors now produces a 20 fold increase in speed in the following
	// loop.
	sd0 = m_objfield.dimensions[0];
	sd1 = m_objfield.dimensions[1];
	sd2 = m_objfield.dimensions[2];
	ts0 = m_objfield.triangle_size[0];
	ts1 = m_objfield.triangle_size[1];

	int face = 0;
	int j = 0;
	for (i = 0; i < m_totalStars; i++) { 
		// If a star is in our probability range, we'll modify it.
		prob = (double)rand()/(double)RAND_MAX*100.0;


		// If the coherence factor is higher than a random number chosen between
		// 0 and 100, then we don't do anything to the star.  This means that
		// (100-coherence)% of the total stars will change.
		if (m_objfield.probability < prob) {
			if(m_objfield.bodyType == 0.0){	// solid body
				while(1){ // try until we find base inside Ellipsoid
					// Find a random point to base our triangle around.
					baseX = (double)rand()/(double)RAND_MAX*sd0 - sd0/2.0;
					baseY = (double)rand()/(double)RAND_MAX*sd1 - sd1/2.0;
					baseZ = (double)rand()/(double)RAND_MAX*sd2 - sd2/2.0;

					if(m_objfield.shape == 1.0) break; // cuboid
					else if(m_objfield.shape == 0.0 && // Ellipsoid: x^2/a^2+y^2/a^2+z^2/a^2=1
						4.0*baseX*baseX/sd0/sd0+4.0*baseY*baseY/sd1/sd1+4.0*baseZ*baseZ/sd2/sd2 <= 1){
						break;
					}
				}
			}
			else if(m_objfield.bodyType == 1.0){	// hollow body (surface area)
				if(m_objfield.shape == 0.0){ // Ellipsoid
					// Find a random point on hollow sphere (Yes, don't know how to evenly distribute star on ellipsoid)
					double ele = asin(((double)rand()/(double)RAND_MAX)*2-1);	//-90(-1) <= elevation <= 90(1); The Lambert Projection
					double azi = ((double)rand()/(double)RAND_MAX)*2*PI;		//0 <= azimth <= 360
					e.x = cos(ele) * cos(azi);
					e.y = sin(ele);
					e.z = cos(ele) * sin(azi);
					baseX = sd0/2.0 * e.x;
					baseY = sd1/2.0 * e.y;
					baseZ = sd2/2.0 * e.z;
				}
				else if(m_objfield.shape == 1.0){ // Cuboid
					// Find a random point to base our triangle around.
					baseX = (double)rand()/(double)RAND_MAX*sd0 - sd0/2.0;
					baseY = (double)rand()/(double)RAND_MAX*sd1 - sd1/2.0;
					baseZ = (double)rand()/(double)RAND_MAX*sd2 - sd2/2.0;

					face = rand()%6;
					switch ( face )
					{
						case 0: // front face
							baseZ = sd2/2.0;
							break;
						case 1: // back face
							baseZ = -sd2/2.0;
							break;
						case 2: // left face
							baseX = sd0/2.0;
							break;
						case 3: // right face
							baseX = -sd0/2.0;
							break;
						case 4: // top face
							baseY = sd1/2.0;
							break;
						case 5: // bottom face
							baseY = -sd1/2.0;
							break;
					}
				}
			}
			else if(m_objfield.bodyType == 2.0){	// texture mapping
				if(m_objfield.shape == 0.0){ // Ellipsoid
					// Find a random point to base our triangle around.
					double ele = asin(((double)rand()/(double)RAND_MAX)*2-1);	//-90(-1) <= elevation <= 90(1); The Lambert Projection
					baseX = (double)rand()/(double)RAND_MAX*m_textureWidthCM - m_textureWidthCM/2.0;
					baseY = (m_textureHeightCM/2.0) * ele/(PI/2.0);
					baseZ = sd2/2.0;
				}
				else if(m_objfield.shape == 1.0){ //cuboid
					// Find a random point to base our triangle around.
					baseX = (double)rand()/(double)RAND_MAX*m_textureWidthCM - m_textureWidthCM/2.0;
					baseY = (double)rand()/(double)RAND_MAX*m_textureHeightCM - m_textureHeightCM/2.0;
					baseZ = sd2/2.0;
				}
			}

			j = AddObjStar(m_objfield.starType, baseX, baseY, baseZ, ts0, ts1, j);
		}
		else{
			// draw point
			if(m_objfield.starType == 0.0) j = j + 3;
			// draw triangle
			else if(m_objfield.starType == 1.0) j = j + 9;
		}
	}

	if(m_objfield.bodyType == 2.0){	// texture mapping
		GenerateObjectTexture(LEFT_EYE);
		GenerateObjectTexture(RIGHT_EYE);
	}
}

GLvoid GLObject::DrawTextureEllipsoid(int whichEye, double ax, double by, double cz, int nAzi, int nEle, double theta1, double theta2, double phi1, double phi2)
{
	int i,j;
	double t1,t2,t3;
	XYZ3 e,p;

	glDisable(GL_BLEND);
	glEnable(GL_TEXTURE_2D);
	if (whichEye == LEFT_EYE)
		glBindTexture(GL_TEXTURE_2D,m_objTextureL); 
	else
		glBindTexture(GL_TEXTURE_2D,m_objTextureR);

	for (j=0;j<nEle;j++) {
		t1 = phi1 + j * (phi2 - phi1) / nEle;
		t2 = phi1 + (j + 1) * (phi2 - phi1) / nEle;

		glBegin(GL_QUAD_STRIP);
		//glBegin(GL_TRIANGLE_STRIP);

		for (i=0;i<=nAzi;i++) {
			t3 = theta1 + i * (theta2 - theta1) / nAzi;

			e.x = cos(t1) * cos(t3);
			e.y = sin(t1);
			e.z = -cos(t1) * sin(t3);
			p.x = ax * e.x;
			p.y = by * e.y;
			p.z = cz * e.z;
			//glNormal3f(e.x,e.y,e.z);
			glTexCoord2f(i/(double)nAzi,j/(double)nEle);
			glVertex3f(p.x,p.y,p.z);

			e.x = cos(t2) * cos(t3);
			e.y = sin(t2);
			e.z = -cos(t2) * sin(t3);
			p.x = ax * e.x;
			p.y = by * e.y;
			p.z = cz * e.z;
			//glNormal3f(e.x,e.y,e.z);
			glTexCoord2f(i/(double)nAzi,(j+1)/(double)nEle);
			glVertex3f(p.x,p.y,p.z);
		}

		glEnd();
	}

	glEnable(GL_BLEND);
	glDisable(GL_TEXTURE_2D);
}

GLvoid GLObject::DrawTextureCuboid(int whichEye, double width, double height, double depth)
{
	glDisable(GL_BLEND);
	glEnable(GL_TEXTURE_2D);
	if (whichEye == LEFT_EYE)
		glBindTexture(GL_TEXTURE_2D,m_objTextureL); 
	else
		glBindTexture(GL_TEXTURE_2D,m_objTextureR);

	double tx[4], ty[4];
	tx[0] = 0;
	tx[1] = depth/m_textureProjWidthCM;
	tx[2] = (width+depth)/m_textureProjWidthCM;
	tx[3] = 1;
	ty[0] = 0;
	ty[1] = depth/m_textureProjHeightCM;
	ty[2] = (height+depth)/m_textureProjHeightCM;
	ty[3] = 1;

	glBegin(GL_QUADS);
		// Front Face
		glNormal3f( 0.0f, 0.0f, 1.0f);
		glTexCoord2f(tx[1], ty[1]); glVertex3f(-width/2.0, -height/2.0,  depth/2.0);
		glTexCoord2f(tx[2], ty[1]); glVertex3f( width/2.0, -height/2.0,  depth/2.0);
		glTexCoord2f(tx[2], ty[2]); glVertex3f( width/2.0,  height/2.0,  depth/2.0);
		glTexCoord2f(tx[1], ty[2]); glVertex3f(-width/2.0,  height/2.0,  depth/2.0);
		// Back Face
		glNormal3f( 0.0f, 0.0f, -1.0f);
		glTexCoord2f(tx[1], ty[1]); glVertex3f( width/2.0, -height/2.0, -depth/2.0);
		glTexCoord2f(tx[2], ty[1]); glVertex3f(-width/2.0, -height/2.0, -depth/2.0);
		glTexCoord2f(tx[2], ty[2]); glVertex3f(-width/2.0,  height/2.0, -depth/2.0);
		glTexCoord2f(tx[1], ty[2]); glVertex3f( width/2.0,  height/2.0, -depth/2.0);
		// Top Face
		glNormal3f( 0.0f, 1.0f, 0.0f);
		glTexCoord2f(tx[1], ty[2]); glVertex3f(-width/2.0, height/2.0,  depth/2.0);
		glTexCoord2f(tx[2], ty[2]); glVertex3f( width/2.0, height/2.0,  depth/2.0);
		glTexCoord2f(tx[2], ty[3]); glVertex3f( width/2.0, height/2.0, -depth/2.0);
		glTexCoord2f(tx[1], ty[3]); glVertex3f(-width/2.0, height/2.0, -depth/2.0);
		// Bottom Face
		glNormal3f( 0.0f,-1.0f, 0.0f);
		glTexCoord2f(tx[1], ty[0]); glVertex3f(-width/2.0, -height/2.0, -depth/2.0);
		glTexCoord2f(tx[2], ty[0]); glVertex3f( width/2.0, -height/2.0, -depth/2.0);
		glTexCoord2f(tx[2], ty[1]); glVertex3f( width/2.0, -height/2.0,  depth/2.0);
		glTexCoord2f(tx[1], ty[1]); glVertex3f(-width/2.0, -height/2.0,  depth/2.0);
		// Right Face
		glNormal3f( 1.0f, 0.0f, 0.0f);
		glTexCoord2f(tx[2], ty[1]); glVertex3f( width/2.0, -height/2.0,  depth/2.0);
		glTexCoord2f(tx[3], ty[1]); glVertex3f( width/2.0, -height/2.0, -depth/2.0);
		glTexCoord2f(tx[3], ty[2]); glVertex3f( width/2.0,  height/2.0, -depth/2.0);
		glTexCoord2f(tx[2], ty[2]); glVertex3f( width/2.0,  height/2.0,  depth/2.0);
		// Left Face
		glNormal3f(-1.0f, 0.0f, 0.0f);
		glTexCoord2f(tx[0], ty[1]); glVertex3f(-width/2.0, -height/2.0, -depth/2.0);
		glTexCoord2f(tx[1], ty[1]); glVertex3f(-width/2.0, -height/2.0,  depth/2.0);
		glTexCoord2f(tx[1], ty[2]); glVertex3f(-width/2.0,  height/2.0,  depth/2.0);
		glTexCoord2f(tx[0], ty[2]); glVertex3f(-width/2.0,  height/2.0, -depth/2.0);
	glEnd();

	glEnable(GL_BLEND);
	glDisable(GL_TEXTURE_2D);
}

GLvoid GLObject::RenderObj(int whichEye)
{
	if(m_objfield.starType == 0.0) glPointSize(m_objfield.dotSize);
	if (whichEye == LEFT_EYE){
		glColor3d(m_objfield.starLeftColor[0] * m_objfield.luminance * m_dynLumRatio,	// Red
				m_objfield.starLeftColor[1] * m_objfield.luminance * m_dynLumRatio,		// Green
				m_objfield.starLeftColor[2] * m_objfield.luminance * m_dynLumRatio);	// Blue
	}
	else{
		glColor3d(m_objfield.starRightColor[0] * m_objfield.luminance * m_dynLumRatio,	// Red
			m_objfield.starRightColor[1] * m_objfield.luminance * m_dynLumRatio,		// Green
			m_objfield.starRightColor[2] * m_objfield.luminance * m_dynLumRatio);		// Blue
	}

	glPushMatrix();
		DrawObject(whichEye);
	glPopMatrix();
	
	//draw object copies
	for(int i=0; i<m_objfield.numOfCopy; i++){
		glPushMatrix();
			glTranslated(m_objfield.copyObjTrans.at(i).x, m_objfield.copyObjTrans.at(i).y, m_objfield.copyObjTrans.at(i).z);
			DrawObject(whichEye);
		glPopMatrix();
	}
}

GLvoid GLObject::DrawObject(int whichEye)
{
	glTranslated(m_objFieldTran.x, m_objFieldTran.y, m_objFieldTran.z);
	glRotated(m_rotAngle, m_rotVector.x, m_rotVector.y, m_rotVector.z);
	glRotated(m_objfield.initRotAngle , m_rotVector.x, m_rotVector.y, m_rotVector.z);

	if(m_objfield.bodyType == 2.0){	// texture mapping
		if(m_objfield.shape == 0.0){ // Ellipsoid
			DrawTextureEllipsoid(whichEye, 
				m_objfield.dimensions.at(0)/2.0,
				m_objfield.dimensions.at(1)/2.0,
				m_objfield.dimensions.at(2)/2.0,
				m_objfield.textureSegment.at(0),
				m_objfield.textureSegment.at(1),
				PI/2.0, 2.0*PI+PI/2.0, -PI/2.0, PI/2.0);
		}
		else if(m_objfield.shape == 1.0){ // cuboid
			//DrawObjectStarField();
			DrawTextureCuboid(whichEye, 
				m_objfield.dimensions.at(0),
				m_objfield.dimensions.at(1),
				m_objfield.dimensions.at(2));
		}
	}
	else DrawObjectStarField();
}


void GLObject::SetObjFieldTran(double x, double y, double z)
{
	m_objFieldTran.x = x;
	m_objFieldTran.y = y;
	m_objFieldTran.z = z;
}

void GLObject::SetObjRotAngle(double rotAngle)
{
	m_rotAngle = rotAngle;
}

XYZ3 GLObject::GetObjOrigin()
{
	XYZ3 origin;
	origin.x = m_objfield.origin.at(0);
	origin.y = m_objfield.origin.at(1);
	origin.z = m_objfield.origin.at(2);
	return origin;
}
