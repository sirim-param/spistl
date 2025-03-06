#ifndef STL_H
#define STL_H

//==========================================================
class ModelData
{
protected:
	class ModelSize
	{
	private:
		float _center_x, _center_y, _center_z;
		float _size_x, _size_y, _size_z;
	public:
		ModelSize(float cx, float cy, float cz, float sx, float sy, float sz)
			: _center_x(cx), _center_y(cy), _center_z(cz)
			, _size_x(sx), _size_y(sy), _size_z(sz)
		{
		}
		float cx() const { return _center_x; }
		float cy() const { return _center_y; }
		float cz() const { return _center_z; }
		float sx() const { return _size_x; }
		float sy() const { return _size_y; }
		float sz() const { return _size_z; }
		float size() const { return max(_size_x, max(_size_y, _size_z)); }
	};

protected:
	HDC m_hDC;
	BITMAPINFO m_bmi;
	HBITMAP m_hBMP;
	HGLRC m_hRC;

	bool Begin(int w, int h);
	void End(char* buf);

	void drawAxes(float cx = 0.0f, float cy = 0.0f, float cz = 0.0f);
	void drawFrame(int x, int y, int w, int h);
	void drawModel(const char* data, const ModelSize& cc,  int x, int y, int w, int h, float angle, float rx, float ry, float rz);

	void setFrustum(int w, int h, float d);
	void setOrtho(int w, int h);

public:
	
	ModelData();
	int Render(const char*data, char* buf, int w, int h);

	virtual const ModelSize GetSize(const char* data) = 0;
	virtual int SetModel(const char* data) = 0;
};


//==========================================================
class Stl : public ModelData
{
public:
	virtual const ModelSize GetSize(const char* data);
	virtual int SetModel(const char* data);
};

//==========================================================
#include <tinyxml.h>
#include <vector>

class Model : public ModelData
{
private:
	struct Vertex
	{
		float x, y, z;
		Vertex(float px = 0.0f, float py = 0.0f, float pz = 0.0f)
			: x(px), y(py), z(pz)
		{ }
		const Vertex& operator-(const Vertex& v)
		{
			return Vertex(x - v.x, y - v.y, z - v.z);
		}
	};
	struct Polygon
	{
		Vertex v1, v2, v3, n;
		Polygon(const Vertex& p1, const Vertex& p2, const Vertex& p3)
			: v1(p1), v2(p2), v3(p3)
		{
			Vertex v21 = v2 - v1;
			Vertex v31 = v3 - v1;

			n =	Vertex
			(
				v21.y * v31.z - v21.z * v31.y,
				v21.z * v31.x - v21.x * v31.z,
				v21.x * v31.y - v21.y * v31.x
			);
			float l = 1.0f / sqrt(n.x * n.x + n.y * n.y + n.z * n.z);
			n.x *= l;
			n.y *= l;
			n.z *= l;
		}
	};
	bool analyzed;
	std::vector<Vertex> v;
	std::vector<Polygon> p;

	void AnalyzeXml(TiXmlElement* elem);
	bool Analyze(const char* data);

public:
	Model() : analyzed(false) {}
	virtual const ModelSize GetSize(const char* data);
	virtual int SetModel(const char* data);
};

#endif //STL_H

