#include "pch.h"

#include "stl.h"
#include <math.h>
#include <gl/GL.h>


void ModelData::drawAxes(float cx, float cy, float cz)
{
    glDisable(GL_LIGHTING);

    glLineWidth(2.0f);

    // X軸 (赤)
    glColor3f(1.0f, 0.0f, 0.0f);
    glBegin(GL_LINES);
    glVertex3f(cx, cy, cz);
    glVertex3f(cx+1000.0f, cy, cz);
    glEnd();

    // Y軸 (緑)
    glColor3f(0.0f, 1.0f, 0.0f);
    glBegin(GL_LINES);
    glVertex3f(cx, cy, cz);
    glVertex3f(cx, 1000.0f + cy, cz);
    glEnd();

    // Z軸 (青)
    glColor3f(0.0f, 0.0f, 1.0f);
    glBegin(GL_LINES);
    glVertex3f(cx, cy, cz);
    glVertex3f(cx, cy, 1000.0f+cz);
    glEnd();

    glEnable(GL_LIGHTING);
}

void ModelData::drawFrame(int x, int y, int w, int h)
{
    glBegin(GL_LINE_LOOP);
    {
        glVertex2i(x, y);
        glVertex2i(x + w, y);
        glVertex2i(x + w, y + h);
        glVertex2i(x, y + h);
    }
    glEnd();
}
void ModelData::drawModel(const char* data, const ModelSize& cc, int x, int y, int w, int h, float angle, float rx, float ry, float rz)
{
    //左下
    glViewport(x, y, w, h);
    glPushMatrix();
    {
        //モデル回転
        glRotatef(angle, rx, ry, rz);
        //カメラ逆算的にワールドを移動
        glTranslatef(-cc.cx(), -cc.cy(), -cc.cz());
        //モデル描画
        glBegin(GL_TRIANGLES);
        {
            SetModel(data);
        }
        glEnd();
        //座標軸描画
        drawAxes(cc.cx(), cc.cy(), cc.cz());
    }
    glPopMatrix();
}

void ModelData::setFrustum(int w, int h, float d)
{
    //Projection行列を初期化
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    float aspect = (float)w / (float)h;
    float fov = 45.0f;
    float zNear = 0.01f;
    float zFar = 10000.0f;
    float fH = tan(fov / 360 * 3.14159f) * zNear;
    float fW = fH * aspect;
    glFrustum(-fW, fW, -fH, fH, zNear, zFar);
    glTranslatef(0, 0, d);

    //Model行列を設定
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
}
void ModelData::setOrtho(int w, int h)
{
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, w, 0, h, -1, 1);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

}

//==========================================================
bool ModelData::Begin(int w, int h)
{
    // デバイスコンテキスト(HDC)の作成
    m_hDC = CreateCompatibleDC(NULL);

    // ビットマップの作成
    m_bmi = {
        sizeof(BITMAPINFOHEADER),
        w,
        h,
        1,
        24,
        BI_RGB,
        0, 0, 0, 0, 0,
    };
    void* pvBits;
    m_hBMP = CreateDIBSection(m_hDC, &m_bmi, DIB_RGB_COLORS, &pvBits, NULL, 0);
    if (m_hBMP == nullptr)
        return false;

    // HDCにビットマップを設定
    SelectObject(m_hDC, m_hBMP);

    // ピクセルフォーマットの設定
    PIXELFORMATDESCRIPTOR pfd = {
        sizeof(PIXELFORMATDESCRIPTOR),
        1,
        PFD_DRAW_TO_BITMAP | PFD_SUPPORT_OPENGL | PFD_SUPPORT_GDI,
        PFD_TYPE_RGBA,
        24,
        0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0,
        24,
        0,
        0,
        PFD_MAIN_PLANE,
        0,
        0, 0, 0,
    };
    int pixFormat = ChoosePixelFormat(m_hDC, &pfd);
    SetPixelFormat(m_hDC, pixFormat, &pfd);

    // レンダリングコンテキスト(RC)の作成と設定
    m_hRC = wglCreateContext(m_hDC);
    wglMakeCurrent(m_hDC, m_hRC);


    //バッファ初期化
    glClearColor(0.2f, 0.2f, 0.2f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    //Zの有効化と、環境光の有効化
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    float ambient[] = { 0.6f, 0.3f, 0.2f, 1.0f };
    glLightfv(GL_LIGHT0, GL_AMBIENT, ambient);

    return true;
}
void ModelData::End(char* buf)
{
    //終わり
    glFlush();

    // ここでレンダリング結果を処理（例：保存や画像処理）
    GetDIBits(m_hDC, m_hBMP, 0, m_bmi.bmiHeader.biHeight, buf, (BITMAPINFO*)&m_bmi, DIB_RGB_COLORS);

    // リソースの解放
    wglMakeCurrent(NULL, NULL);
    wglDeleteContext(m_hRC);
    DeleteObject(m_hBMP);
    DeleteDC(m_hDC);
}


//==========================================================
ModelData::ModelData()
    : m_hDC(NULL)
    , m_hBMP(NULL)
    , m_hRC(NULL)
{
    m_bmi = {
        sizeof(BITMAPINFOHEADER),
        0,
        0,
        1,
        24,
        BI_RGB,
        0, 0, 0, 0, 0,
    };
}

int ModelData::Render(const char* data, char* buf, int w, int h)
{
    const ModelSize& cc = GetSize(data);

    Begin(w, h);

    //描画サイズ
    int sw = h / 3; //3分割の正方形にするためh
    int sh = h / 3;

    setFrustum(w - sw, h, - (cc.size() * 2.0f));

    //Y0が下
    drawModel(data, cc, sw, 0, w - sw, h, -45.0f, 1.0f, 0.0f, 1.0f);

    setFrustum(sw, sh, -(cc.size() * 2.0f));

    drawModel(data, cc, 0, 0, sw, sh, 0.0f, 0.0f, 0.0f, 0.0f);
    drawModel(data, cc, 0, sh*2, sw, sh, -90.0f, 1.0f, 0.0f, 0.0f);
    drawModel(data, cc, 0, sh, sw, sh, 90.0f, 0.0f, 1.0f, 0.0f);

    //枠描画
    setOrtho(w, h);
    {
        glDisable(GL_LIGHTING);
        glLineWidth(3.0f);
        glColor3f(1.0f, 1.0f, 1.0f);

        glViewport(0, 0, w, h);

        drawFrame(0, 0, sw, sh);
        drawFrame(0, sh, sw, sh);
        drawFrame(0, sh*2, sw, sh);
    }

    End(buf);

    return 0;
}

//==========================================================
const ModelData::ModelSize Stl::GetSize(const char* data)
{
    int count = *(int*)&data[80];
    int idx = 84; //84bytesからポリゴンデータが入ってる

    float min_x =  100000.0f;
    float min_y =  100000.0f;
    float min_z =  100000.0f;
    float max_x = -100000.0f;
    float max_y = -100000.0f;
    float max_z = -100000.0f;
    for (int i = 0; i < count; ++i)
    {
        float* fp = (float*)&data[idx];

        if (fp[3] < min_x) min_x = fp[3];
        if (fp[6] < min_x) min_x = fp[6];
        if (fp[9] < min_x) min_x = fp[9];

        if (fp[4] < min_y) min_y = fp[4];
        if (fp[7] < min_y) min_y = fp[7];
        if (fp[10] < min_y) min_y = fp[10];

        if (fp[5] < min_z) min_z = fp[5];
        if (fp[8] < min_z) min_z = fp[8];
        if (fp[11] < min_z) min_z = fp[11];


        if (fp[3] > max_x) max_x = fp[3];
        if (fp[6] > max_x) max_x = fp[6];
        if (fp[9] > max_x) max_x = fp[9];

        if (fp[4] > max_y) max_y = fp[4];
        if (fp[7] > max_y) max_y = fp[7];
        if (fp[10] > max_y) max_y = fp[10];

        if (fp[5] > max_z) max_z = fp[5];
        if (fp[8] > max_z) max_z = fp[8];
        if (fp[11] > max_z) max_z = fp[11];
        idx += 50;
    }

    float dx = max_x - min_x;
    float dy = max_y - min_y;
    float dz = max_z - min_z;

    return ModelSize(min_x + dx * 0.5f, min_y + dy * 0.5f, min_z + dz * 0.5f, dx, dy, dz);
}

int Stl::SetModel(const char* data)
{
    int count = *(int*)&data[80];
    int idx = 84; //84bytesからポリゴンデータが入ってる

    for (int i = 0; i < count; ++i)
    {
        float* fp = (float*)&data[idx];

        glNormal3f(fp[0], fp[1], fp[2]);
        glVertex3f(fp[3], fp[4], fp[5]);
        glVertex3f(fp[6], fp[7], fp[8]);
        glVertex3f(fp[9], fp[10], fp[11]);

        idx += 50;
    }

    return 0;
}


//==========================================================
#include <tinyxml.h>

void Model::AnalyzeXml(TiXmlElement* elem)
{
    if (!elem) return;

    if (!strcmp(elem->Value(), "vertex"))
    {
        float x = 0.0f, y = 0.0f, z = 0.0f;
        for (TiXmlAttribute* attr = elem->FirstAttribute(); attr; attr = attr->Next())
        {
            if (!strcmp(attr->Name(), "x")) x = (float)atof(attr->Value());
            if (!strcmp(attr->Name(), "y")) y = (float)atof(attr->Value());
            if (!strcmp(attr->Name(), "z")) z = (float)atof(attr->Value());
        }
        v.push_back(Vertex(x, y, z));
    }

    if (!strcmp(elem->Value(), "triangle"))
    {
        int v1 = -1, v2 = -1, v3 = -1;
        for (TiXmlAttribute* attr = elem->FirstAttribute(); attr; attr = attr->Next())
        {
            if (!strcmp(attr->Name(), "v1")) v1 = atoi(attr->Value());
            if (!strcmp(attr->Name(), "v2")) v2 = atoi(attr->Value());
            if (!strcmp(attr->Name(), "v3")) v3 = atoi(attr->Value());
        }
        if (v1 >= 0 && v2 >= 0 && v3 >= 0)
        {
            p.push_back(Polygon(v[v1], v[v2], v[v3]));
        }
    }

    // 子要素を再帰的に解析
    for (TiXmlElement* child = elem->FirstChildElement(); child; child = child->NextSiblingElement())
    {
        AnalyzeXml(child);
    }
}

bool Model::Analyze(const char* data)
{
    if (analyzed) return true;

    TiXmlDocument doc;
    doc.Parse(data);
    if (doc.Error()) return false;

    TiXmlElement* root = doc.RootElement();
    if(root)
    {
        for (TiXmlElement* child = root->FirstChildElement(); child; child = child->NextSiblingElement())
        {
            AnalyzeXml(child);
        }
    }
    analyzed = true;
    return true;
}


const ModelData::ModelSize Model::GetSize(const char* data)
{
    if (Analyze(data))
    {
        float min_x = 100000.0f;
        float min_y = 100000.0f;
        float min_z = 100000.0f;
        float max_x = -100000.0f;
        float max_y = -100000.0f;
        float max_z = -100000.0f;
        for (auto it = v.begin(); it != v.end(); ++it)
        {
            if (min_x > it->x) min_x = it->x;
            if (min_y > it->y) min_y = it->y;
            if (min_z > it->z) min_z = it->z;

            if (max_x < it->x) max_x = it->x;
            if (max_y < it->y) max_y = it->y;
            if (max_z < it->z) max_z = it->z;
        }
        float dx = max_x - min_x;
        float dy = max_y - min_y;
        float dz = max_z - min_z;

        return ModelSize(min_x + dx * 0.5f, min_y + dy * 0.5f, min_z + dz * 0.5f, dx, dy, dz);
    }
    return ModelSize(0.0f, 0.0f, 0.0f, 100.0f, 100.0f, 100.0f);
}

int Model::SetModel(const char* data)
{
    if (Analyze(data))
    {
        for (auto it = p.begin(); it != p.end(); ++it)
        {
            glNormal3f(it->n.x, it->n.y, it->n.z);
            glVertex3f(it->v1.x, it->v1.y, it->v1.z);
            glVertex3f(it->v2.x, it->v2.y, it->v2.z);
            glVertex3f(it->v3.x, it->v3.y, it->v3.z);
        }
    }
    return 0;
}