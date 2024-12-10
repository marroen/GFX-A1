#pragma once

#define NUM_MESHES 9 // 4 for Dragons, 9 for Rips, 16 for Teapots
#define SHOULD_MOVE false
#define HALF_MIRRORED true

namespace Tmpl8
{

// application class
class WhittedApp : public TheApp
{
public:
	// game flow methods
	void Init();
	void AnimateScene();
	float3 Trace( Ray& ray, RayCounter* counter, int rayDepth = 0 );
	void Tick( float deltaTime );
	void Shutdown() { /* implement if you want to do something on exit */ }
	// input handling
	void MouseUp( int button ) { /* implement if you want to detect mouse button presses */ }
	void MouseDown( int button ) { /* implement if you want to detect mouse button presses */ }
	void MouseMove( int x, int y ) { mousePos.x = x, mousePos.y = y; }
	void MouseWheel( float y ) { /* implement if you want to handle the mouse wheel */ }
	void KeyUp( int key ) { /* implement if you want to handle keys */ }
	void KeyDown( int key ) { /* implement if you want to handle keys */ }
	// data members
	int2 mousePos;
	Mesh* mesh;
	BVHInstance bvhInstance[256];
	TLAS tlas;
	float3 p0, p1, p2; // virtual screen plane corners
	float3 camPos;
	float3* accumulator;
	RayCounter* counters[524288] = { nullptr };
	uint counterIdx = 0;
	Timer timer;
	float* skyPixels;
	int skyWidth, skyHeight, skyBpp;
};

} // namespace Tmpl8