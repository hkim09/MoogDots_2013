#ifndef AFFINE4MATRIX_H_
#define AFFINE4MATRIX_H_

#include <iostream>
#include <vector>
#include <math.h>

using namespace std;

//3D Classes for Graphics
//@@@@@@@@@@@@@@@@@@ Point3 class @@@@@@@@@@@@@@@@
class Point3{ 
public: 
	double x,y,z;
	void set(double dx, double dy, double dz){x = dx; y = dy; z = dz;}
	void set(Point3& p){x = p.x; y = p.y; z = p.z;} 	
	Point3(double xx, double yy, double zz){x = xx; y = yy; z = zz;}
	Point3(){x = y = z = 0;}
	void build4tuple(double v[])
	{// load 4-tuple with this color: v[3] = 1 for homogeneous
		v[0] = x; v[1] = y; v[2] = z; v[3] = 1.0f;
	}	
	Point3 operator+(Point3& P){ return Point3(x+P.x, y+P.y, z+P.z); }
	Point3 operator-(Point3& P){ return Point3(x-P.x, y-P.y, z-P.z); }
	Point3 operator*(double d){ return Point3(x*d, y*d, z*d); }
	Point3 operator/(double d){ return Point3(x/d, y/d, z/d); }
};
//@@@@@@@@@@@@@@@@@@ Vector3 class @@@@@@@@@@@@@@@@
class Vector3{ 
public: 
	double x,y,z;
	void set(double dx, double dy, double dz){ x = dx; y = dy; z = dz;} 
	void set(Vector3& v){ x = v.x; y = v.y; z = v.z;}
	void flip(){x = -x; y = -y; z = -z;} // reverse this vector
	void setDiff(Point3& a, Point3& b)//set to difference a - b
	{ x = a.x - b.x; y = a.y - b.y; z = a.z - b.z;} //vector from b point to a
	void normalize()//adjust this vector to unit length
	{		
		double sizeSq = x * x + y * y + z * z;
		if(sizeSq < 0.0000001)
		{
			//cerr << "\nnormalize() sees vector (0,0,0)!";
			return; // does nothing to zero vectors;
		}
		double scaleFactor = 1.0/(double)sqrt(sizeSq);
		x *= scaleFactor; y *= scaleFactor; z *= scaleFactor;
	}
	Vector3(double xx, double yy, double zz){x = xx; y = yy; z = zz;}
	Vector3(Vector3& v){x = v.x; y = v.y; z = v.z;}
	Vector3(){x = y = z = 0;} //default constructor
	Vector3 cross(Vector3 b) //return this cross b
	{
	   Vector3 c(y*b.z - z*b.y, z*b.x - x*b.z, x*b.y - y*b.x);
	   return c;
	}
	double dot(Vector3 b) // return this dotted with b
	{return x * b.x + y * b.y + z * b.z;}
	void build4tuple(double v[])
	{// v[3] = 0 for homogeneous
		v[0] = x; v[1] = y; v[2] = z; v[3] = 0.0f;
	}	
	double magnitude(){return sqrt(x * x + y * y + z * z);}

	Vector3 operator*(double d){ return Vector3(x*d, y*d, z*d); }
	Vector3 operator/(double d){ return Vector3(x/d, y/d, z/d); }
	Vector3 operator-(){ return Vector3(-x, -y, -z); }

};

class Affine4Matrix// manages homogeneous affine transformations
{
public:
	double m[16]; // hold a 4 by 4 matrix
	
	Affine4Matrix() // make identity transform
	{
		setIdentityMatrix();
	}

	void setIdentityMatrix() // make identity transform
	{
		m[0] = m[5]  = m[10] = m[15] = 1.0;
		m[1] = m[2]  = m[3]  = m[4]  = 0.0;
		m[6] = m[7]  = m[8]  = m[9]  = 0.0;
		m[11]= m[12] = m[13] = m[14] = 0.0;
	}

	// setup rotation matrix according to rotation vector and rotation angle (rad); P.241, equation 5.33
	void setRotationMatrix(Vector3& rotVector, double angle)
	{
		double c = cos(angle), s = sin(angle);
		Vector3 u(rotVector);
		u.normalize();
		m[0] = c+(1-c)*u.x*u.x;		m[4] = (1-c)*u.y*u.x-s*u.z; m[8] = (1-c)*u.z*u.x+s*u.y; m[12] = 0.0;
		m[1] = (1-c)*u.x*u.y+s*u.z;	m[5] = c+(1-c)*u.y*u.y;		m[9] = (1-c)*u.z*u.y-s*u.x;	m[13] = 0.0;
		m[2] = (1-c)*u.x*u.z-s*u.y;	m[6] = (1-c)*u.y*u.z+s*u.x; m[10] = c+(1-c)*u.z*u.z;	m[14] = 0.0;
		m[3] = 0.0;					m[7] = 0.0;					m[11] = 0.0;				m[15] = 1.0;
	}

	// P.234,
	void setTranslationMatrix(double x, double y, double z)
	{
		m[0] = 1.0;	m[4] = 0.0; m[8] = 0.0;	 m[12] = x;
		m[1] = 0.0; m[5] = 1.0; m[9] = 0.0;	 m[13] = y;
		m[2] = 0.0; m[6] = 0.0; m[10] = 1.0; m[14] = z;
		m[3] = 0.0; m[7] = 0.0; m[11] = 0.0; m[15] = 1.0;
	}

	//Find matrix such that convert world coord to camera coordinate
	//Reference: P.365 & P.367 fig. 7.11
	void setWorld2CameraMatrix(Point3 eye, Point3 look, Vector3 up)
	{
		Vector3 n, u, v;
		n.setDiff(eye, look);	n.normalize();	//n vector from look --> eye.
		u.set(up.cross(n));		u.normalize();	//u = up x n
		v.set(n.cross(u));						//v = n x u

		Vector3 eVec(eye.x, eye.y, eye.z);
		m[0] = u.x;	m[4] = u.y; m[8] = u.z;	 m[12] = -eVec.dot(u); // camera x-coord
		m[1] = v.x; m[5] = v.y; m[9] = v.z;	 m[13] = -eVec.dot(v); // camera y-coord
		m[2] = n.x; m[6] = n.y; m[10] = n.z; m[14] = -eVec.dot(n); // camera z-coord
		m[3] = 0.0; m[7] = 0.0; m[11] = 0.0; m[15] = 1.0;
	}

	//http://graphics.stanford.edu/courses/cs248-98-fall/Final/q4.html
	//Invert the transformations:
	//Let M be the 4x4 Matrix, Mr be the rotation Matrix and Mt be the tranlation Matrix
	//M = Mt*Mr -> M^-1 = (Mt*Mr)^-1 = Mr^-1 * Mt^-1 = Mr' * Mt^-1
	Affine4Matrix invertTranformMatrix()
	{
		Affine4Matrix invMrot, invMtran;
		
		invMrot.set(this->m);
		invMrot.m[12] = invMrot.m[13] = invMrot.m[14] = 0.0;
		invMrot.transpose();
		invMtran.setTranslationMatrix(-m[12], -m[13], -m[14]); 
		return invMrot*invMtran;
	}

	void set(Affine4Matrix a) // set this matrix to a
	{
		for(int i = 0; i < 16; i++)
			m[i]=a.m[i];
	}

	void set(double *a) // overloaded
	{
		for(int i = 0; i < 16; i++)
			m[i]=a[i];
	}

	void set(const double *a) // overloaded
	{
		for(int i = 0; i < 16; i++)
			m[i]=a[i];
	}

	// Transpose matrix
	void transpose()
	{
		Affine4Matrix tmp(*this); 
		m[0] = tmp.m[0];  m[4] = tmp.m[1];  m[8]  = tmp.m[2];   m[12] = tmp.m[3];
		m[1] = tmp.m[4];  m[5] = tmp.m[5];  m[9]  = tmp.m[6];   m[13] = tmp.m[7];
		m[2] = tmp.m[8];  m[6] = tmp.m[9];  m[10] = tmp.m[10];  m[14] = tmp.m[11];
		m[3] = tmp.m[12]; m[7] = tmp.m[13]; m[11] = tmp.m[14];  m[15] = tmp.m[15];
	}

	// overloading funtion - Matrix multiplication
	Affine4Matrix operator*( const Affine4Matrix& A)
	{
		Affine4Matrix M;
		double sum;

		for(int c = 0; c < 4; c++)
			for(int r = 0; r <4 ; r++)
			{
				sum = 0;
				for(int k = 0; k < 4; k++) 
					sum += this->m[4*k+r]*A.m[4*c+k];
				M.m[4*c+r] = sum;
			}

		return M;
	}

	// overloading funtion - 
	Affine4Matrix operator-()
	{
		Affine4Matrix M;
		for(int i = 0; i < 16; i++) M.m[i] = -m[i];
		return M;
	}

	//<<<<<<<<matrix*point>>>>>>>>>>>>
	Point3 operator*(Point3& oldPoint){
		double v[4]={0,0,0,0};
		double nv[4]={0,0,0,0};
		oldPoint.build4tuple(v);
		for(int r = 0; r <4 ; r++){
			for( int c=0; c<4; c++){
				nv[r] += this->m[c*4+r] * v[c];
			}
		}
		return Point3(nv[0],nv[1],nv[2]);  
	}

	//<<<<<<<<matrix*vector>>>>>>>>>>>>
	Vector3 operator*(Vector3& oldVector){
		double v[4]={0,0,0,0};
		double nv[4]={0,0,0,0};
		oldVector.build4tuple(v);
		for(int r = 0; r <4 ; r++){
			for( int c=0; c<4; c++){
				nv[r] += this->m[c*4+r] * v[c];
			}
		}
		return Vector3(nv[0],nv[1],nv[2]);
	}
};

class Ray3{
public:
	Point3 point;		// starting point
	Vector3 direction;	// line or ray direction
	//double t;			// parameter t that distinguishes one point on the line from another

	Ray3()
	{
		point.set(0,0,0);
		direction.set(0,0,0);
	}

	Ray3(Point3 p, Vector3 v)
	{
		point.set(p);
		direction.set(v);
		direction.normalize();

	}

	Point3 RayPosition(double t) 
	{
		Point3 position;
		position.x = point.x + t*direction.x;
		position.y = point.y + t*direction.y;
		position.z = point.z + t*direction.z;
		return position;
	}
};

class ConvexPlanePolygon3{
public:
	Point3 *points;			// P0, P1, P2, ... 
	Vector3 *lineDirs;		// D0 = P1-P0, D1 = P2-P1, ...
	Vector3 *outwardNormals;	// N0 is normal of D0, N1 is normal of D1, ... and plane polygen P.189

	Vector3 planeNormal;	// normal of the plane polygon in count-clockwise direction of points.
	int numOfPoint;

	ConvexPlanePolygon3(vector<Point3> p)
	{
		numOfPoint = (int)p.size();
		points = new Point3[numOfPoint];
		lineDirs = new Vector3[numOfPoint];
		outwardNormals = new Vector3[numOfPoint];

		// set line directors
		Vector3 v;
		int i;
		for(i=0; i<(int)p.size()-1; i++)
		{
			points[i].set(p.at(i));
			v.setDiff(p.at(i+1),p.at(i));
			lineDirs[i].set(v);
			lineDirs[i].normalize();
		}
		points[i].set(p.at(i));
		v.setDiff(p.at(0),p.at(i));
		lineDirs[i].set(v);
		lineDirs[i].normalize();

		planeNormal.set(lineDirs[0].cross(lineDirs[1]));
		planeNormal.normalize();

		// set outward normals - useful for Clipping P.189
		for(i=0; i<(int)p.size(); i++) 
		{
			outwardNormals[i].set(lineDirs[i].cross(planeNormal));
			outwardNormals[i].normalize();
		}
	}

	~ConvexPlanePolygon3()
	{
		delete points;
		delete lineDirs;
		delete outwardNormals;
	}
};

#endif