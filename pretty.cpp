#include "precomp.h"
#include "bvh.h"
#include "pretty.h"

// THIS SOURCE FILE:
// Code for the article "How to Build a BVH", part 7: consolidation.
// This version shows how to ray trace a textured mesh, with basic
// shading using interpolated normals and a Lambertian material.
// Feel free to copy this code to your own framework. Absolutely no
// rights are reserved. No responsibility is accepted either.
// For updates, follow me on twitter: @j_bikker.

TheApp* CreateApp() { return new PrettyApp(); }

// PrettyApp implementation

void PrettyApp::Init()
{
	Timer timer;
	timer.reset();
	bool expired = false;

	Mesh* mesh = new Mesh( "assets/teapot.obj", "assets/bricks.png" );
	for (int i = 0; i < 16; i++)
		bvhInstance[i] = BVHInstance( mesh->bvh, i );
	//bvhInstance[0] = BVHInstance( mesh->bvh, 0 );
	tlas = TLAS( bvhInstance, 16 );
	// setup screen plane in world space
	float aspectRatio = (float)SCRWIDTH / SCRHEIGHT;
	p0 = TransformPosition( float3( -aspectRatio, 1, 2 ), mat4::RotateX( 0.5f ) );
	p1 = TransformPosition( float3( aspectRatio, 1, 2 ), mat4::RotateX( 0.5f ) );
	p2 = TransformPosition( float3( -aspectRatio, -1, 2 ), mat4::RotateX( 0.5f ) );
	camPos = float3( 0, 3, -6.5f );
	// create a floating point accumulator for the screen
	accumulator = new float3[SCRWIDTH * SCRHEIGHT];
}

void PrettyApp::AnimateScene()
{
	// animate the scene
	static float a[16] = {0}, h[16] = {5, 4, 3, 2, 1, 5, 4, 3}, s[16] = {0};
	for (int i = 0, x = 0; x < 4; x++) for (int y = 0; y < 4; y++, i++)
	{
		mat4 R, T = mat4::Translate( (x - 1.5f) * 2.5f, 0, (y - 1.5f) * 2.5f );
		/*if ((x + y) & 1) R = mat4::RotateY(a[i]);
		else R = mat4::Translate( 0, h[i / 2], 0 );
		if ((a[i] += (((i * 13) & 7) + 2) * 0.005f) > 2 * PI) a[i] -= 2 * PI;
		if ((s[i] -= 0.01f, h[i] += s[i]) < 0) s[i] = 0.2f;*/
		bvhInstance[i].SetTransform( T * R * mat4::Scale( 1.5f ) );
	}
	// update the TLAS
	tlas.BuildQuick();
}

float3 PrettyApp::Trace( Ray& ray, RayCounter* counter )
{
	tlas.Intersect( ray, counter );
	Intersection i = ray.hit;
	if (i.t == 1e30f) return float3( 0 );
	return float3( i.u, i.v, 1 - (i.u + i.v) );
}

void PrettyApp::Tick( float deltaTime )
{
	// update the TLAS
	AnimateScene();

	// render the scene: multithreaded tiles
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
				(p1 - p0) * ((x * 8 + u) / (float)SCRWIDTH) +
				(p2 - p0) * ((y * 8 + v) / (float)SCRHEIGHT);
			ray.D = normalize( pixelPos - ray.O );
			ray.hit.t = 1e30f; // 1e30f denotes 'no hit'
			uint pixelAddress = x * 8 + u + (y * 8 + v) * SCRWIDTH;
			accumulator[pixelAddress] = Trace( ray, counter );
		}
	}

	// convert the floating point accumulator into pixels
	for( int i = 0; i < SCRWIDTH * SCRHEIGHT; i++ )
	{
		int r = min( 255, (int)(255 * accumulator[i].x) );
		int g = min( 255, (int)(255 * accumulator[i].y) );
		int b = min( 255, (int)(255 * accumulator[i].z) );
		screen->pixels[i] = (r << 16) + (g << 8) + b;
	}

	// Periodically print information about counters when certain conditions are met
	if (timer.elapsed() >= 60) {  // After 10 seconds
		float triangleTestsAverage;
		float minTriangleTests = 999999;
		float maxTriangleTests = -1;

		float boxTestsAverage;
		float minBoxTests = 999999;
		float maxBoxTests = -1;

		float minTraversals = 999999;
		float maxTraversals = -1;

		float totalTriangleTests = 0;
		float totalBoxTests = 0;
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

		std::cout << "TotalTraversals: " << totalTraversals << std::endl;
		std::cout << "MinTraversals: " << minTraversals << std::endl;
		std::cout << "MaxTraversals: " << maxTraversals << std::endl;
		std::cout << "AverageTraversals: " << totalTraversals / length << std::endl;

		timer.reset();
	}
}

// EOF