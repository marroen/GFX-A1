## Build
In order to run the project, you should run on a Windows PC with a CPU not too ancient.
Beyond that, just open the '_bvhdemo' solution in Visual Studio, select 'whitted' as the startup project and run it.

You may further recreate the experiments of the report by adjusting arguments in 'whitted.h' and 'whitted.cpp'.
In 'whitted.cpp', the relevant arguments are in 'void WhittedApp::Init()'.
Using invalid arguments may cause crashes, and using wrong combination of camera position and meshes may cause you to not be able to see the meshes.

## TODOs

05.12
- [x] 10k mesh
- [x] intersection tests
- [x] traversal steps

06.12
- [x] multiple camera angles
- [x] start report
- [x] average
- [x] minimum
- [x] maximum

08.12
- [x] create two more scenes
- [x] identify all ray types in codebase
- [x] track all ray types

09.12
- [x] do tests per scene
- [x] write report