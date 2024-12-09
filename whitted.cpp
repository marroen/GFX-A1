#include "precomp.h"
#include "bvh.h"
#include "whitted.h"

// THIS SOURCE FILE:
// Code for the article "How to Build a BVH", part 8: Whitted.
// This version shows how to build a simple Whitted-style ray tracer
// as a test case for the BVH code of the previous articles. This is
// also the final preparation for the GPGPU code in article 9.
// Feel free to copy this code to your own framework. Absolutely no
// rights are reserved. No responsibility is accepted either.
// For updates, follow me on twitter: @j_bikker.

TheApp* CreateApp() { return new WhittedApp(); }

inline float3 RGB8toRGB32F( uint c )
{
	float s = 1 / 256.0f;
	int r = (c >> 16) & 255;
	int g = (c >> 8) & 255;
	int b = c & 255;
	return float3( r * s, g * s, b * s );
}

// WhittedApp implementation

void WhittedApp::Init()
{
	Timer timer;
	timer.reset();

	float3 camPos1 = float3(0, -2, -8.5f);
	float3 camPos2 = float3(0, 100, -150.0f);
	float3 camPos3 = float3(0, 15, -30.0f);

	camPos = camPos1;

	//mesh = new Mesh( "assets/teapot.obj", "assets/bricks.png" );
	//mesh = new Mesh( "assets/dragon.obj", "assets/bricks.png" );
	//mesh = new Mesh("assets/avant-garde.obj", "assets/bricks.png");
	//mesh = new Mesh("assets/dash-of-color.obj", "assets/bricks.png");
	mesh = new Mesh("assets/rip.obj", "assets/bricks.png");
	for (int i = 0; i < NUM_MESHES; i++)
		bvhInstance[i] = BVHInstance( mesh->bvh, i );
	tlas = TLAS( bvhInstance, NUM_MESHES );
	// create a floating point accumulator for the screen
	accumulator = new float3[SCRWIDTH * SCRHEIGHT];
	// load HDR sky
	int bpp = 0;
	skyPixels = stbi_loadf( "assets/sky_19.hdr", &skyWidth, &skyHeight, &skyBpp, 0 );
	for (int i = 0; i < skyWidth * skyHeight * 3; i++) skyPixels[i] = sqrtf( skyPixels[i] );
}

void WhittedApp::AnimateScene()
{
	// animate the scene
	static float a[16] = { 0 }, h[16] = { 5, 4, 3, 2, 1, 5, 4, 3 }, s[16] = { 0 };
	for (int i = 0, x = 0; x < std::sqrt(NUM_MESHES); x++) for (int y = 0; y < std::sqrt(NUM_MESHES); y++, i++)
	{
		mat4 R, T = mat4::Translate( (x - 1.5f) * 2.5f, 0, (y - 1.5f) * 2.5f );
		if (SHOULD_MOVE)
		{
			if ((x + y) & 1) R = mat4::RotateY(a[i]);
			else R = mat4::Translate(0, h[i / 2], 0);
			if ((a[i] += (((i * 13) & 7) + 2) * 0.005f) > 2 * PI) a[i] -= 2 * PI;
			if ((s[i] -= 0.01f, h[i] += s[i]) < 0) s[i] = 0.2f;
		}
		bvhInstance[i].SetTransform( T * R * mat4::Scale( 1.5f ) );
	}
	// update the TLAS
	tlas.BuildQuick();
}

float3 WhittedApp::Trace( Ray& ray, RayCounter* counter, int rayDepth )
{
	tlas.Intersect( ray, counter );
	Intersection i = ray.hit;
	if (i.t == 1e30f)
	{	
		// sample sky
		uint u = (uint)(skyWidth * atan2f( ray.D.z, ray.D.x ) * INV2PI - 0.5f);
		uint v = (uint)(skyHeight * acosf( ray.D.y ) * INVPI - 0.5f);
		uint skyIdx = (u + v * skyWidth) % (skyWidth * skyHeight);
		return 0.65f * float3( skyPixels[skyIdx * 3], skyPixels[skyIdx * 3 + 1], skyPixels[skyIdx * 3 + 2] );
	}
	// calculate texture uv based on barycentrics
	uint triIdx = i.instPrim & 0xfffff;
	uint instIdx = i.instPrim >> 20;
	TriEx& tri = mesh->triEx[triIdx];
	Surface* tex = mesh->texture;
	float2 uv = i.u * tri.uv1 + i.v * tri.uv2 + (1 - (i.u + i.v)) * tri.uv0;
	int iu = (int)(uv.x * tex->width) % tex->width;
	int iv = (int)(uv.y * tex->height) % tex->height;
	uint texel = tex->pixels[iu + iv * tex->width];
	float3 albedo = RGB8toRGB32F( texel );
	// calculate the normal for the intersection
	float3 N = i.u * tri.N1 + i.v * tri.N2 + (1 - (i.u + i.v)) * tri.N0;
	N = normalize( TransformVector( N, bvhInstance[instIdx].GetTransform() ) );
	float3 I = ray.O + i.t * ray.D;
	// shading
	bool mirror = (instIdx * 17) & 1;
	if (mirror && HALF_MIRRORED)
	{	
		// calculate the specular reflection in the intersection point
		counter->incrementBounces();
		Ray secondary;
		secondary.D = ray.D - 2 * N * dot( N, ray.D );
		secondary.O = I + secondary.D * 0.001f;
		secondary.hit.t = 1e30f;
		if (rayDepth >= 10) return float3( 0 );
		return Trace( secondary, counter, rayDepth + 1 );
	}
	else
	{
		// calculate the diffuse reflection in the intersection point
		float3 lightPos( 3, 10, 2 );
		float3 lightColor( 150, 150, 120 );
		float3 ambient( 0.2f, 0.2f, 0.4f );
		float3 L = lightPos - I;
		float dist = length( L );
		L *= 1.0f / dist;
		return albedo * (ambient + max( 0.0f, dot( N, L ) ) * lightColor * (1.0f / (dist * dist)));
	}
}

void WhittedApp::Tick( float deltaTime )
{
	// update the TLAS
	AnimateScene();
	// render the scene: multithreaded tiles
	static float angle = 0;// angle += 0.01f;
	mat4 M1 = mat4::RotateY( angle ), M2 = M1 * mat4::RotateX( -0.65f );
	// setup screen plane in world space
	float aspectRatio = (float)SCRWIDTH / SCRHEIGHT;
	p0 = TransformPosition( float3( -aspectRatio, 1, 1.5f ), M2 );
	p1 = TransformPosition( float3( aspectRatio, 1, 1.5f ), M2 );
	p2 = TransformPosition( float3( -aspectRatio, -1, 1.5f ), M2 );
	camPos = TransformPosition( camPos, M1 );
#pragma omp parallel for schedule(dynamic)
	for (int tile = 0; tile < (SCRWIDTH * SCRHEIGHT / 64); tile++)
	{
		// render an 8x8 tile
		int x = tile % (SCRWIDTH / 8), y = tile / (SCRWIDTH / 8);
		Ray ray;
		ray.O = camPos;
		RayCounter* counter = new RayCounter(ray);

		// Critical section to update counters safely
#pragma omp critical
		{
			if (counterIdx < 524288)
			{
				counters[counterIdx] = counter;
				counterIdx++;
			}

		}

		for (int v = 0; v < 8; v++) for (int u = 0; u < 8; u++)
		{
			// setup a primary ray
			float3 pixelPos = ray.O + p0 +
				(p1 - p0) * ((x * 8 + u + RandomFloat()) / SCRWIDTH) +
				(p2 - p0) * ((y * 8 + v + RandomFloat()) / SCRHEIGHT);
			ray.D = normalize( pixelPos - ray.O );
			ray.hit.t = 1e30f; // 1e30f denotes 'no hit'
			uint pixelAddress = x * 8 + u + (y * 8 + v) * SCRWIDTH;
			accumulator[pixelAddress] = Trace( ray , counter );
		}
	}
	// convert the floating point accumulator into pixels
	for (int i = 0; i < SCRWIDTH * SCRHEIGHT; i++)
	{
		int r = min( 255, (int)(255 * accumulator[i].x) );
		int g = min( 255, (int)(255 * accumulator[i].y) );
		int b = min( 255, (int)(255 * accumulator[i].z) );
		screen->pixels[i] = (r << 16) + (g << 8) + b;
	}

	// Periodically print information about counters when certain conditions are met
	if (timer.elapsed() >= 60) {  // After 10 seconds
		float minTriangleTests = 999999;
		float maxTriangleTests = -1;

		float minBoxTests = 999999;
		float maxBoxTests = -1;

		float maxBounces = -1;

		float averageTraversals;
		float minTraversals = 999999;
		float maxTraversals = -1;

		float totalTriangleTests = 0;
		float totalBoxTests = 0;
		float totalBounces = 0;
		float totalTraversals = 0;

		uint length = sizeof(counters) / sizeof(counters[0]);
		for (int i = 0; i < length; i++)
		{
			//std::cout << "triangleTests: " << counters[i]->triangleTests << std::endl;
			//std::cout << "boxTests: " << counters[i]->boxTests << std::endl;
			// Triangle tests
			if (counters[i]->triangleTests < minTriangleTests)
			{
				minTriangleTests = counters[i]->triangleTests;
			}
			if (counters[i]->triangleTests > maxTriangleTests)
			{
				maxTriangleTests = counters[i]->triangleTests;
			}
			// Box tests
			if (counters[i]->boxTests < minBoxTests)
			{
				minBoxTests = counters[i]->boxTests;
			}
			if (counters[i]->boxTests > maxBoxTests)
			{
				maxBoxTests = counters[i]->boxTests;
			}
			// Bounces
			if (counters[i]->bounces > maxBounces)
			{
				maxBounces = counters[i]->bounces;
			}
			// Traversals
			if (counters[i]->traversals < minTraversals)
			{
				minTraversals = counters[i]->traversals;
			}
			if (counters[i]->traversals > maxTraversals)
			{
				maxTraversals = counters[i]->traversals;
			}
			totalTriangleTests = totalTriangleTests + counters[i]->triangleTests;
			totalBoxTests = totalBoxTests + counters[i]->boxTests;
			totalBounces = totalBounces + counters[i]->bounces;
			totalTraversals = totalTraversals + counters[i]->traversals;

		}
		std::cout << length << " rays fired." << std::endl;

		std::cout << "TotalTriangleTests: " << totalTriangleTests << std::endl;
		std::cout << "MinTriangleTests: " << minTriangleTests << std::endl;
		std::cout << "MaxTriangleTests: " << maxTriangleTests << std::endl;
		std::cout << "AverageTriangleTests " << totalTriangleTests / length << std::endl;

		std::cout << "TotalBoxTests: " << totalBoxTests << std::endl;
		std::cout << "MinBoxTests: " << minBoxTests << std::endl;
		std::cout << "MaxBoxTests: " << maxBoxTests << std::endl;
		std::cout << "AverageBoxTests " << totalBoxTests / length << std::endl;

		std::cout << "TotalBounces: " << totalBounces << std::endl;
		std::cout << "MaxBounces: " << maxBounces << std::endl;
		std::cout << "AverageBounces " << totalBounces / length << std::endl;

		std::cout << "TotalTraversals: " << totalTraversals << std::endl;
		std::cout << "MinTraversals: " << minTraversals << std::endl;
		std::cout << "MaxTraversals: " << maxTraversals << std::endl;
		std::cout << "AverageTraversals: " << totalTraversals / length << std::endl;

		timer.reset();
	}
}

// EOF