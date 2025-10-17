@echo off

echo Building Point Cloud Generator...

REM Create build directory
if not exist build mkdir build
cd build

REM Configure and build
cmake ..
cmake --build . --config Release

REM Check if build was successful
if %ERRORLEVEL% EQU 0 (
    echo.
    echo Build successful!
    echo   Generator: .\build\Release\point_cloud_generator.exe
    echo   Inspector: .\build\Release\inspect_pointcloud.exe
    echo.
    echo Usage examples:
    echo   .\build\Release\point_cloud_generator.exe                    # Generate 10M points
    echo   .\build\Release\point_cloud_generator.exe 100000000          # Generate 100M points
    echo   .\build\Release\point_cloud_generator.exe 50000000 test.pcd  # Generate 50M points to test.pcd
    echo.
    echo   .\build\Release\inspect_pointcloud.exe test.pcd              # Inspect point cloud file
    echo   .\build\Release\inspect_pointcloud.exe test.pcd --detailed   # Show detailed info
) else (
    echo Build failed!
    exit /b 1
)

