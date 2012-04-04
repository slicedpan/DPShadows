#include <GL\glew.h>
#include <GL\glfw.h>
#include <svl\SVL.h>
#include "Shader.h"
#include "ShaderManager.h"
#include <FPSCamera\FPSCamera.h>
#include <FPSCamera\CameraController.h>
#include "FrameBufferObject.h"
#include "VBOMesh.h"
#include <cstdlib>
#include <math.h>
#include "glm\glm.h"
#include "QuadDrawer.h"
#include "BasicTexture.h"

bool running = true;
bool limitFPS = true;
bool shadowBlur = false;

#define BUFFER_OFFSET(i) ((char*)NULL + i)

void CreateVBOs();

unsigned int vertexBuf;
unsigned int indexBuf;

int width = 800;
int height = 600;

bool keyState[256];
bool lastKeyState[256];

bool lightControl = false;

FPSCamera* camera;
CameraController* controller;

FPSCamera* lightCam;

Shader* shadowGen;
Shader* copyTex;
Shader* basic;
Shader* normBasic;
Shader* renderToGBuffer;
Shader* finalDraw;
ShaderManager* shaderManager;

BasicTexture* noiseTexture;

Mat4 Projection;

int glMajorVersion;
int glMinorVersion;
int glRev;

bool debugDraw = false;
bool drawGBuf = false;
bool animateLight = false;

Mat4 lightWorldView;
float lightYaw = 0.0f;
float lightPitch = 0.0f;

float lightRadius;

GLuint texID;

int downsamplePasses = 0;

double lastTime;

float RandomFloat()
{
	return (float)rand() / RAND_MAX;
}

FrameBufferObject* shadowMap;
FrameBufferObject* shadowMap2;
FrameBufferObject* gBuf;
FrameBufferObject* mainScene;

void CreateFBOs()
{
	shadowMap = new FrameBufferObject(1024, 1024, 24, 0, GL_RG32F, GL_TEXTURE_2D);
	shadowMap->AttachTexture("first", GL_LINEAR, GL_LINEAR_MIPMAP_LINEAR);

	shadowMap2 = new FrameBufferObject(768, 768, 0, 0, GL_RG32F, GL_TEXTURE_2D);
	shadowMap2->AttachTexture("first", GL_LINEAR, GL_LINEAR);

	gBuf = new FrameBufferObject(width, height, 0, 0, GL_RGB, GL_TEXTURE_2D);
	gBuf->AttachDepthTexture("depth", GL_LINEAR, GL_LINEAR, GL_DEPTH_COMPONENT32);
	gBuf->AttachTexture("normal");	

	mainScene = new FrameBufferObject(width, height, 0, 0, GL_RGB, GL_TEXTURE_2D);
	mainScene->AttachTexture("colour");

	if (!shadowMap->CheckCompleteness())
		throw;
}

VBOMesh* mesh;

void setup()
{
	srand(42);
	camera = new FPSCamera();
	camera->Position[2] = 5.0f;
	camera->Position[1] = 0.5f;
	camera->Position[0] = 0.5f;
	camera->SetZFar(100.0f);
	controller = new CameraController();
	controller->SetCamera(camera);
	controller->MaxSpeed = 0.03;
	controller->PitchAngularVelocity *= 10.0f;
	controller->YawAngularVelocity *= 10.0f;
	lightCam = new FPSCamera();
	lightCam->SetZNear(0.001f);
	lightRadius = 25.0f;
	lightCam->SetZFar(lightRadius);
	lightCam->Pitch = M_PI / 2.0f;
	lightCam->Position[1] = 15.0f;
	GLenum err = glewInit();
	if (err != GLEW_OK)
	{
		printf("%s", glewGetErrorString(err));
		return;	
	}	
	basic = new Shader("Assets/Shaders/basic.vert", "Assets/Shaders/varianceBasic.frag", "Variance Shadow Map Basic");
	//normBasic = new Shader("Assets/Shaders/basic.vert", "Assets/Shaders/normBasic.frag", "Normal + Depth Shadow Map Basic");
	copyTex = new Shader("Assets/Shaders/copy.vert", "Assets/Shaders/copy.frag", "Copy");
	shadowGen = new Shader("Assets/Shaders/shadowGen.vert", "Assets/Shaders/shadowGen.frag", "Shadowmap Generator");
	renderToGBuffer = new Shader("Assets/Shaders/gBuf.vert", "Assets/Shaders/gBuf.frag", "Render to GBuffer");
	finalDraw = new Shader("Assets/Shaders/copy.vert", "Assets/Shaders/final.frag", "Final Reconstruction");
	ShaderManager::GetSingletonPtr()->CompileShaders();

	noiseTexture = new BasicTexture("Assets/Textures/rgnoisehi.png");
	noiseTexture->Load();

	mesh = new VBOMesh("Assets/Meshes/sponza.obj", false, true);	
	mesh->Load();

	glBindTexture(GL_TEXTURE_2D, noiseTexture->GetId());
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glBindTexture(GL_TEXTURE_2D, 0);
	CreateFBOs();
	memset(keyState, 0, sizeof(bool) * 256);
	memset(lastKeyState, 0, sizeof(bool) * 256);	
}

double frameBegin;
int frameCount;
double timeCount;
int currentFps;
int dX, dY;

Vec3 lightPos(0.0, 30.0, 0.0);
double lightPosFactor = 0.0f;

void update()
{
	++frameCount;
	frameBegin = glfwGetTime();
	double elapsedTime = frameBegin - lastTime;
	timeCount += elapsedTime;
	char buf[100];
	if (timeCount > 1.0)
	{
		currentFps = frameCount;
		frameCount = 0;
		timeCount = 0.0;	

	}
	sprintf(buf, "FPS: %d, GL Version %d.%d, rev %d. downsamples: %d", currentFps, glMajorVersion, glMinorVersion, glRev, downsamplePasses);
	glfwSetWindowTitle(buf);
	if (keyState['W'])
		controller->MoveForward();
	if (keyState['S'])
		controller->MoveBackward();
	if (keyState['A'])
		controller->MoveLeft();
	if (keyState['D'])
		controller->MoveRight();
	if (keyState['C'])
		controller->MoveDown();
	if (keyState[' '])
		controller->MoveUp();	
	controller->Update(elapsedTime * 1000.0f);
	controller->ChangePitch(-elapsedTime * dY);
	controller->ChangeYaw(-elapsedTime * dX);
	dY = 0;
	dX = 0;	

	if (animateLight)
		lightPosFactor += elapsedTime * (1.0 / 30.0);

	if (lightPosFactor > 1.0)
		lightPosFactor -= 1.0;

	lightPos[0] = sin(lightPosFactor * 2 * M_PI) * 5.0;
	lightPos[1] = cos(lightPosFactor * 2 * M_PI) * 5.0 + 10.0;

	lightCam->Position = lightPos;
}


void display()
{
	glClearColor(0.0, 0.0, 0.0, 0.0);
	glClearDepth(1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);	
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK);
	glDisable(GL_TEXTURE_2D);

	shadowMap->Bind();

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LEQUAL);
	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK);

	shadowGen->Use();
	shadowGen->Uniforms("lightWorldView").SetValue(lightCam->GetViewTransform());
	shadowGen->Uniforms("lightRadius").SetValue(lightRadius);
	
	Mat4 world = Mat4(vl_one);
	basic->Uniforms("lightRadius").SetValue(lightRadius);

	mesh->Draw();

	shadowMap->Unbind();

	glBindTexture(GL_TEXTURE_2D, shadowMap->GetTexture(0));
	glGenerateMipmap(GL_TEXTURE_2D);

	Mat4 projViewWorld = world * camera->GetViewTransform() * camera->GetProjectionMatrix();

	gBuf->Bind();
	glEnable(GL_DEPTH_TEST);
	glDepthMask(GL_TRUE);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	renderToGBuffer->Use();
	renderToGBuffer->Uniforms("projViewWorld").SetValue(projViewWorld);
	mesh->Draw();
	gBuf->Unbind();

	for (int i = 0; i < downsamplePasses; ++i)
	{
		shadowMap2->Bind();
		copyTex->Use();
		copyTex->Uniforms("baseTex").SetValue(0);
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, shadowMap->GetTexture(0));
		QuadDrawer::DrawQuad(Vec2(-1.0f, -1.0f), Vec2(1.0f, 1.0f));
		shadowMap2->Unbind();
		shadowMap->Bind();
		glBindTexture(GL_TEXTURE_2D, shadowMap2->GetTexture(0));
		QuadDrawer::DrawQuad(Vec2(-1.0f, -1.0f), Vec2(1.0f, 1.0f));		
		shadowMap->Unbind();
	}

	mainScene->Bind();
	glClear(GL_COLOR_BUFFER_BIT);
	basic->Use();
	basic->Uniforms("View").SetValue(camera->GetViewTransform());
	basic->Uniforms("Projection").SetValue(camera->GetProjectionMatrix());	
	
	basic->Uniforms("World").SetValue(world);
	basic->Uniforms("lightPos").SetValue(lightCam->Position);
	basic->Uniforms("lightRadius").SetValue(lightRadius);
	basic->Uniforms("lightCone").SetValue(lightCam->GetForwardVector());
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, shadowMap->GetTexture("first"));
	glGenerateMipmap(GL_TEXTURE_2D);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, noiseTexture->GetId());
	glActiveTexture(GL_TEXTURE2);
	glBindTexture(GL_TEXTURE_2D, gBuf->GetTexture("depth"));
	basic->Uniforms("gDepth").SetValue(2);
	basic->Uniforms("invScreenWidth").SetValue(1.0f / (float)width);
	basic->Uniforms("invScreenHeight").SetValue(1.0f / (float)height);
	basic->Uniforms("shadowTex").SetValue(0);
	basic->Uniforms("noiseTex").SetValue(1);
	basic->Uniforms("noiseTexSize").SetValue(512.0f);
	basic->Uniforms("lightWorldView").SetValue(lightCam->GetViewTransform());



	/*
	glBegin(GL_TRIANGLES);
	glNormal3f(1.0, 0.2, 0.5);
	glVertex3f(0.0, 0.0, 0.0);
	glNormal3f(0.0, 1.0, 0.2);
	glVertex3f(1.0, 0.0, 0.0);
	glNormal3f(0.3, 0.0, 0.7);
	glVertex3f(1.0, 1.0, 0.0);
	glEnd();*/
	
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glMultMatrixf(camera->GetProjectionMatrix().Ref());
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	glMultMatrixf(camera->GetViewTransform().Ref());

	if (debugDraw)
		mesh->DrawImmediate();
	else
		mesh->Draw();

	mainScene->Unbind();

	glUseProgram(0);

	glBegin(GL_LINES);
	glColor3f(1.0, 0.0, 0.0);
	glVertex3f(0.0, 0.0, 0.0);
	glVertex3f(1.0, 0.0, 0.0);
	glColor3f(0.0, 1.0, 0.0);
	glVertex3f(0.0, 0.0, 0.0);
	glVertex3f(0.0, 1.0, 0.0);
	glColor3f(0.0, 0.0, 1.0);
	glVertex3f(0.0, 0.0, 0.0);
	glVertex3f(0.0, 0.0, 1.0);
	glEnd();

	finalDraw->Use();
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, mainScene->GetTexture(0));
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, gBuf->GetTexture("depth"));

	finalDraw->Uniforms("baseTex").SetValue(0);
	finalDraw->Uniforms("gDepth").SetValue(1);

	QuadDrawer::DrawQuad(Vec2(-1.0, -1.0), Vec2(1.0, 1.0), Vec2(1.0f / width, 1.0f / height));
	

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, shadowMap->GetTexture("first"));

	copyTex->Use();
	copyTex->Uniforms("baseTex").SetValue(0);

	glDisable(GL_DEPTH_TEST);

	if (lightControl)
		QuadDrawer::DrawQuad(Vec2(0.0, 0.0), Vec2(1.0, 1.0));

	if (drawGBuf)
	{
		glBindTexture(GL_TEXTURE_2D, gBuf->GetTexture("depth"));
		QuadDrawer::DrawQuad(Vec2(-1.0, -1.0), Vec2(-0.5, -0.5));
		glBindTexture(GL_TEXTURE_2D, gBuf->GetTexture("normal"));
		QuadDrawer::DrawQuad(Vec2(-0.5, -1.0), Vec2(0.0, -0.5));
	}

	if (limitFPS) 
		glfwSleep(0.016 - glfwGetTime() + frameBegin);
	lastTime = frameBegin;

	glfwSwapBuffers();
}

int Exit()
{
	running = false;
	return 0;
}

bool mouseEnabled = true;
int lastX;
int lastY;

void KeyboardHandler(int keyCode, int state)
{
	if (keyCode < 256)
		keyState[keyCode] = state;
	if (keyCode == 'R' && state == GLFW_PRESS)
		ShaderManager::GetSingletonPtr()->ReloadShaders();
	if(keyCode == 'P' && state == GLFW_PRESS)
		debugDraw = !debugDraw;
	if (keyCode == GLFW_KEY_LSHIFT || keyCode == GLFW_KEY_RSHIFT)
	{
		if (state == GLFW_PRESS)
			controller->MaxSpeed = 0.07f;
		else
			controller->MaxSpeed = 0.03f;
	}
	if (keyCode == 'M' && state == GLFW_PRESS)
	{
		if (mouseEnabled)		
			glfwDisable(GLFW_MOUSE_CURSOR);					
		else
			glfwEnable(GLFW_MOUSE_CURSOR);
		dX = 0;
		dY = 0;
		glfwGetMousePos(&lastX, &lastY);
		mouseEnabled = !mouseEnabled;
	}
	if (keyCode == 'F' && state == GLFW_PRESS)
		limitFPS = !limitFPS;
	if (keyCode == GLFW_KEY_ESC)
		Exit();	
	if (keyCode == 'L' && state == GLFW_PRESS)
	{	
		lightControl = !lightControl;
		if (lightControl)
			controller->SetCamera(lightCam);
		else
			controller->SetCamera(camera);
	}
	if (state == GLFW_PRESS)
	{
		if (keyCode == 'B')
			downsamplePasses -= 1;
		if (keyCode == 'N')
			downsamplePasses += 1;
		if (keyCode == 'O')
			animateLight = !animateLight;
		if (keyCode == 'G')
			drawGBuf = !drawGBuf;
	}
}

void MouseMovementHandler(int x, int y)
{
	if (mouseEnabled)
	{

	}
	else
	{
		dX += x - lastX;
		dY += y - lastY;
		lastX = x;
		lastY = y;
	}
}

int main(int argc, char**argv)
{
	if(!glfwInit())
		return 1;

	lastTime = glfwGetTime();

	glfwOpenWindowHint(GLFW_OPENGL_VERSION_MAJOR, 3);
	glfwOpenWindowHint(GLFW_OPENGL_VERSION_MINOR, 3);

	if (!glfwOpenWindow(800, 600, 8, 8, 8, 8, 24, 8, GLFW_WINDOW))
		return 1;	

	glfwGetGLVersion(&glMajorVersion, &glMinorVersion, &glRev);	
	setup();	

	glfwSetWindowCloseCallback(Exit);
	glfwSetKeyCallback(KeyboardHandler);
	glfwSetMousePosCallback(MouseMovementHandler);

	while(running)
	{
		update();
		display();		
	}

	glfwTerminate();
}