#include <GL\glew.h>
#include <GL\glfw.h>
#include <svl\SVL.h>
#include "Shader.h"
#include "ShaderManager.h"
#include <FPSCamera\FPSCamera.h>
#include <FPSCamera\CameraController.h>
#include "FrameBufferObject.h"
#include "StaticMesh.h"
#include <cstdlib>
#include <math.h>
#include "QuadDrawer.h"
#include "BasicTexture.h"
#include "FBOManager.h"
#include "GLGUI\Primitives.h"

#ifndef M_PI
#define M_PI 3.14159265
#endif

bool running = true;
bool limitFPS = true;
bool shadowBlur = false;
bool DOFEnabled = true;

#define BUFFER_OFFSET(i) ((char*)NULL + i)

#define VBOMesh StaticMesh

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
Shader* SSAO;
Shader* occlusionBlur;
Shader* gaussianBlur;
ShaderManager* shaderManager;

BasicTexture* noiseTexture;

Mat4 Projection;

int occlusionBlurPasses = 2;

int glMajorVersion;
int glMinorVersion;
int glRev;

bool debugDraw = false;
bool drawGBuf = false;
bool animateLight = false;
bool SSAOEnabled = true;

Mat4 lightWorldView;
float lightYaw = 0.0f;
float lightPitch = 0.0f;

float currentDepth = 0.0f;

float lightRadius;

GLuint texID;

int downsamplePasses = 0;
bool drawBuffer = false;
unsigned int currentBuf = 0;


double lastTime;

float RandomFloat()
{
	return (float)rand() / RAND_MAX;
}

FrameBufferObject* shadowMap;
FrameBufferObject* shadowMap2;
FrameBufferObject* gBuf;
FrameBufferObject* mainScene;
FrameBufferObject* mainSceneHalf;
FrameBufferObject* mainSceneHalfBack;
FrameBufferObject* occlusionBuf;
FrameBufferObject* occlusionBack;

void CreateFBOs()
{
	shadowMap = new FrameBufferObject(768, 768, 24, 0, GL_RG32F, GL_TEXTURE_2D, "Shadow Map");
	shadowMap->AttachTexture("first", GL_LINEAR, GL_LINEAR_MIPMAP_LINEAR);

	shadowMap2 = new FrameBufferObject(512, 512, 0, 0, GL_RG32F, GL_TEXTURE_2D, "Shadow Map Downsample");
	shadowMap2->AttachTexture("first", GL_LINEAR, GL_LINEAR);

	gBuf = new FrameBufferObject(width, height, 0, 0, GL_RGB, GL_TEXTURE_2D, "G-Buffer");
	gBuf->AttachDepthTexture("depth", GL_LINEAR, GL_LINEAR, GL_DEPTH_COMPONENT32);
	gBuf->AttachTexture("normal");	

	mainScene = new FrameBufferObject(width, height, 0, 0, GL_RGBA, GL_TEXTURE_2D, "MainScene");
	mainScene->AttachTexture("colour");

	mainSceneHalf = new FrameBufferObject(width / 2, height / 2, 0, 0, GL_RGBA, GL_TEXTURE_2D, "Main Scene Downsample");
	mainSceneHalf->AttachTexture("colour");

	mainSceneHalfBack = new FrameBufferObject(width / 4, height / 4, 0, 0, GL_RGBA, GL_TEXTURE_2D, "Main Scene Downsample Back");
	mainSceneHalfBack->AttachTexture("colour");

	occlusionBuf = new FrameBufferObject(width, height, 0, 0, GL_R16F, GL_TEXTURE_2D, "Ambient Occlusion");
	occlusionBuf->AttachTexture("ao", GL_LINEAR, GL_LINEAR);

	occlusionBack = new FrameBufferObject(width, height, 0, 0, GL_R16F, GL_TEXTURE_2D, "Ambient Occlusion Back-buffer");
	occlusionBack->AttachTexture("ao", GL_LINEAR, GL_LINEAR);

	if (!occlusionBuf->CheckCompleteness())
		throw;

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
	camera->SetFOV((float)height / (float)width);
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
	printf("Loading Shaders...\n");
	basic = new Shader("Assets/Shaders/basic.vert", "Assets/Shaders/varianceBasic.frag", "Variance Shadow Map Basic");
	//normBasic = new Shader("Assets/Shaders/basic.vert", "Assets/Shaders/normBasic.frag", "Normal + Depth Shadow Map Basic");
	copyTex = new Shader("Assets/Shaders/copy.vert", "Assets/Shaders/copy.frag", "Copy");
	shadowGen = new Shader("Assets/Shaders/shadowGen.vert", "Assets/Shaders/shadowGen.frag", "Shadowmap Generator");
	renderToGBuffer = new Shader("Assets/Shaders/gBuf.vert", "Assets/Shaders/gBuf.frag", "Render to GBuffer");
	finalDraw = new Shader("Assets/Shaders/copy.vert", "Assets/Shaders/final.frag", "Final Reconstruction");
	SSAO = new Shader("Assets/Shaders/copy.vert", "Assets/Shaders/SSAO.frag", "SSAO");
	occlusionBlur = new Shader("Assets/Shaders/copy.vert", "Assets/Shaders/occlusionBlur.frag", "Occlusion Buffer Blur");
	gaussianBlur = new Shader("Assets/Shaders/copy.vert", "Assets/Shaders/gaussianblur.frag", "Gaussian Blur");

	ShaderManager::GetSingletonPtr()->CompileShaders();

	printf("Loading Textures...\n");
	noiseTexture = new BasicTexture("Assets/Textures/rgnoisehi.png");
	noiseTexture->Load();

	printf("Textures Loaded\n");

	printf("Loading Meshes...\n");
	mesh = new VBOMesh();
	mesh->LoadObj("Assets/Meshes/sponza.obj", true, true, false);

	glBindTexture(GL_TEXTURE_2D, noiseTexture->GetId());
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glBindTexture(GL_TEXTURE_2D, 0);
	CreateFBOs();
	memset(keyState, 0, sizeof(bool) * 256);
	memset(lastKeyState, 0, sizeof(bool) * 256);

	SetupFont();
}

double frameBegin;
int frameCount;
double timeCount;
int currentFps;
int dX, dY;

Vec3 lightPos(0.0, 30.0, 0.0);
double lightPosFactor = 0.0f;

double elapsedTime = 0.0;

void update()
{
	++frameCount;
	frameBegin = glfwGetTime();
	elapsedTime = frameBegin - lastTime;
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

	mesh->SubMeshes[0]->Draw();

	shadowMap->Unbind();

	glBindTexture(GL_TEXTURE_2D, shadowMap->GetTextureID(0));
	glGenerateMipmap(GL_TEXTURE_2D);

	Mat4 projView = camera->GetViewTransform() * camera->GetProjectionMatrix();
	Mat4 projViewWorld = world * camera->GetViewTransform() * camera->GetProjectionMatrix();

	gBuf->Bind();
	glEnable(GL_DEPTH_TEST);
	glDepthMask(GL_TRUE);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	renderToGBuffer->Use();
	renderToGBuffer->Uniforms("projViewWorld").SetValue(projViewWorld);
	mesh->SubMeshes[0]->Draw();
	float pixels[4];
	glReadPixels(width / 2 - 1, height / 2 - 1, 2, 2, GL_DEPTH_COMPONENT, GL_FLOAT, pixels);
	gBuf->Unbind();
	
	float targetDepth = 0.0;
	for (int i = 0; i < 4; ++i)
	{
		targetDepth += pixels[i];
	}
	targetDepth /= 4.0f;

	currentDepth += (targetDepth - currentDepth) * elapsedTime * 2.0f;

	for (int i = 0; i < downsamplePasses; ++i)
	{
		shadowMap2->Bind();
		copyTex->Use();
		copyTex->Uniforms("baseTex").SetValue(0);
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, shadowMap->GetTextureID(0));
		QuadDrawer::DrawQuad(Vec2(-1.0f, -1.0f), Vec2(1.0f, 1.0f));
		shadowMap2->Unbind();
		shadowMap->Bind();
		glBindTexture(GL_TEXTURE_2D, shadowMap2->GetTextureID(0));
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
	glBindTexture(GL_TEXTURE_2D, shadowMap->GetTextureID("first"));
	glGenerateMipmap(GL_TEXTURE_2D);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, noiseTexture->GetId());
	glActiveTexture(GL_TEXTURE2);
	glBindTexture(GL_TEXTURE_2D, gBuf->GetTextureID("depth"));
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

	mesh->SubMeshes[0]->Draw();

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


	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, gBuf->GetTextureID("normal"));
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, gBuf->GetTextureID("depth"));
	glActiveTexture(GL_TEXTURE2);
	glBindTexture(GL_TEXTURE_2D, noiseTexture->GetId());

	if (SSAOEnabled)
	{			
		occlusionBuf->Bind();
		glClear(GL_COLOR_BUFFER_BIT);
		SSAO->Use();
		SSAO->Uniforms("gNormal").SetValue(0);
		SSAO->Uniforms("gDepth").SetValue(1);
		SSAO->Uniforms("noiseTex").SetValue(2);
		SSAO->Uniforms("pixSize").SetValue(Vec2(2.0f / width, 2.0f / height));
		SSAO->Uniforms("invViewProj").SetValue(inv(projView));
		QuadDrawer::DrawQuad(Vec2(-1.0, -1.0), Vec2(1.0, 1.0));
		occlusionBuf->Unbind();

		glActiveTexture(GL_TEXTURE0);
		occlusionBlur->Use();
		occlusionBlur->Uniforms("baseTex").SetValue(0);

		for (int i = 0; i < occlusionBlurPasses; ++i)
		{

			occlusionBlur->Uniforms("vertical").SetValue(true);
			occlusionBack->Bind();
			glBindTexture(GL_TEXTURE_2D, occlusionBuf->GetTextureID(0));
			QuadDrawer::DrawQuad(Vec2(-1.0, -1.0), Vec2(1.0, 1.0));
			occlusionBack->Unbind();
			
			occlusionBuf->Bind();
			glBindTexture(GL_TEXTURE_2D, occlusionBack->GetTextureID(0));
			occlusionBlur->Uniforms("vertical").SetValue(false);
			QuadDrawer::DrawQuad(Vec2(-1.0, -1.0), Vec2(1.0, 1.0));
			occlusionBuf->Unbind();
		}
	}

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, mainScene->GetTextureID(0));

	if (DOFEnabled)
	{
		mainSceneHalf->Bind();
		copyTex->Use();
		copyTex->Uniforms("baseTex").SetValue(0);
		QuadDrawer::DrawQuad(Vec2(-1.0, -1.0), Vec2(1.0, 1.0));
		mainSceneHalf->Unbind();

		gaussianBlur->Use();
		gaussianBlur->Uniforms("baseTex").SetValue(0);

		gaussianBlur->Uniforms("maxDist").SetValue(4.0f);

		mainSceneHalfBack->Bind();
		glBindTexture(GL_TEXTURE_2D, mainSceneHalf->GetTextureID(0));
		QuadDrawer::DrawQuad(Vec2(-1.0, -1.0), Vec2(1.0, 1.0));
		mainSceneHalfBack->Unbind();

		mainSceneHalf->Bind();
		glBindTexture(GL_TEXTURE_2D, mainSceneHalfBack->GetTextureID(0));
		QuadDrawer::DrawQuad(Vec2(-1.0, -1.0), Vec2(1.0, 1.0));
		mainSceneHalf->Unbind();

		gaussianBlur->Uniforms("maxDist").SetValue(4.0f);

		mainSceneHalfBack->Bind();
		glBindTexture(GL_TEXTURE_2D, mainSceneHalf->GetTextureID(0));
		QuadDrawer::DrawQuad(Vec2(-1.0, -1.0), Vec2(1.0, 1.0));
		mainSceneHalfBack->Unbind();

		mainSceneHalf->Bind();
		glBindTexture(GL_TEXTURE_2D, mainSceneHalfBack->GetTextureID(0));
		QuadDrawer::DrawQuad(Vec2(-1.0, -1.0), Vec2(1.0, 1.0));
		mainSceneHalf->Unbind();

	}

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, mainScene->GetTextureID(0));

	FBOTexture* tex = 0;
	finalDraw->Use();
	if (drawBuffer)
	{
		copyTex->Use();
		tex = FBOManager::GetSingletonPtr()->GetTexture(currentBuf);
		if (tex)
			glBindTexture(GL_TEXTURE_2D, tex->glID);
	}
	
	
	glActiveTexture(GL_TEXTURE3);
	glBindTexture(GL_TEXTURE_2D, occlusionBuf->GetTextureID(0));
	glActiveTexture(GL_TEXTURE4);
	glBindTexture(GL_TEXTURE_2D, mainSceneHalf->GetTextureID(0));
	glActiveTexture(GL_TEXTURE5);
	glBindTexture(GL_TEXTURE_2D, gBuf->GetTextureID("normal"));

	Shader* current = ShaderManager::GetSingletonPtr()->GetCurrent();

	current->Uniforms("baseTex").SetValue(0);
	current->Uniforms("gDepth").SetValue(1);
	current->Uniforms("noiseTex").SetValue(5);
	current->Uniforms("occlusionBuf").SetValue(3);
	current->Uniforms("halfTex").SetValue(4);
	current->Uniforms("pixSize").SetValue(Vec2(1.0f / width, 1.0f / height));
	current->Uniforms("SSAO").SetValue(SSAOEnabled);
	current->Uniforms("DOF").SetValue(DOFEnabled);
	current->Uniforms("currentDepth").SetValue(currentDepth);

	QuadDrawer::DrawQuad(Vec2(-1.0, -1.0), Vec2(1.0, 1.0), Vec2(1.0f / width, 1.0f / height));	

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, fontTex);//shadowMap->GetTextureID("first"));

	copyTex->Use();
	copyTex->Uniforms("baseTex").SetValue(0);

	glDisable(GL_DEPTH_TEST);

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
	if (state == GLFW_PRESS)
	{
		if (keyCode == 'B')
			downsamplePasses -= 1;
		if (keyCode == 'N')
			downsamplePasses += 1;
		if (keyCode == 'O')
			animateLight = !animateLight;
		if (keyCode == GLFW_KEY_UP)
			++currentBuf;
		if (keyCode == GLFW_KEY_DOWN)
			--currentBuf;
		if (keyCode == 'G')
			drawBuffer = !drawBuffer;
		if (keyCode == 'Y')
			SSAOEnabled = !SSAOEnabled;
		if (keyCode == GLFW_KEY_PAGEDOWN)
			--occlusionBlurPasses;
		if (keyCode == GLFW_KEY_PAGEUP)
			++occlusionBlurPasses;
		if (keyCode == 'U')
			DOFEnabled = !DOFEnabled;
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

	if (!glfwOpenWindow(width, height, 8, 8, 8, 8, 24, 8, GLFW_WINDOW))
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