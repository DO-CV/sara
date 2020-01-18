$cmake_vsver = @{2015="v140"; 2017="v141"; 2019="v160"};
$cmake_vsver2 = @{2015="14"; 2017="15"; 2019="16"};

$vsver = 2017

$build_dirname = "sara-build-vs$vsver"
$cmake_toolset = $cmake_vsver[$vsver]

iex "New-Item -ItemType directory -Path ..\$build_dirname"

# Configure the build solution with CMake.
$cmake_options  = "-DCMAKE_TOOLCHAIN_FILE=C:\vcpkg\scripts\buildsystems\vcpkg.cmake "
$cmake_options += "-DSARA_BUILD_VIDEOIO:BOOL=ON "
$cmake_options += "-DSARA_BUILD_SHARED_LIBS:BOOL=ON "
$cmake_options += "-DSARA_BUILD_SAMPLES:BOOL=ON "
$cmake_options += "-DSARA_BUILD_TESTS:BOOL=ON"

# Invoke CMake command.
$cmake_command  = "cmake -H'.' -B'..\$build_dirname' -G'Visual Studio 15 $vsver Win64' "
$cmake_command += "-T '$cmake_toolset' "
$cmake_command += "'$cmake_options'"
iex "$cmake_command"
