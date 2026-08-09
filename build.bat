@echo off

REM Check CommonLibSSEPath_NG
REM Dependency: https://github.com/CharmedBaryon/CommonLibSSE-NG
if not defined CommonLibSSEPath_NG (
    echo ERROR: CommonLibSSEPath_NG is not set.
    echo Dependency: https://github.com/CharmedBaryon/CommonLibSSE-NG - v3.7.0
    exit /b 1
)

if "%CommonLibSSEPath_NG%"=="" (
    echo ERROR: CommonLibSSEPath_NG is empty.
    exit /b 1
)

REM Check CompiledPluginsPath
if not defined CompiledPluginsPath (
    echo ERROR: CompiledPluginsPath is not set. Set
    exit /b 1
)

if "%CompiledPluginsPath%"=="" (
    echo ERROR: CompiledPluginsPath is empty.
    exit /b 1
)

echo Environment variables look good.
echo CommonLibSSEPath_NG=%CommonLibSSEPath_NG%
echo CompiledPluginsPath=%CompiledPluginsPath%

REM Run CMake
cmake --preset vs2022-windows