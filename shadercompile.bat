@echo off
setlocal

:: List of input:output shader pairs
set "FILES=triangle.slang:triangle.vert.spv:vertexMain:vertex triangle.slang:triangle.frag.spv:fragmentMain:fragment"

:: Loop through each pair
for %%F in (%FILES%) do (
    for /f "tokens=1,2,3,4 delims=:" %%A in ("%%F") do (
        set "INPUT=%%A"
        set "OUTPUT=%%B"
		set "SHADERTYPE=%%C"
		set "SHADERSTAGE=%%D"

        call :processFile
    )
)

goto :eof

:processFile
:: Construct the slangc command once
set "SLANGC_CMD=%VULKAN_SDK%\Bin\slangc %INPUT% -profile spirv_1_4 -matrix-layout-column-major -target spirv -o %OUTPUT% -entry %SHADERTYPE% -stage %SHADERSTAGE% -warnings-disable 39001"


rem    echo %INPUT%, %OUTPUT%, %SHADERTYPE%


:: Check if output file exists
if not exist "%OUTPUT%" (
    echo [%INPUT%] → [%OUTPUT%] not found. Compiling...
    %SLANGC_CMD%
rem echo %SLANGC_CMD%
    goto :eof
)

rem    goto :eof


:: Get timestamps
for %%I in ("%INPUT%") do set "INPUT_TIME=%%~tI"
for %%O in ("%OUTPUT%") do set "OUTPUT_TIME=%%~tO"

:: Convert timestamps to sortable format (YYYYMMDDHHMMSS)
set "INPUT_TIME=%INPUT_TIME:~6,4%%INPUT_TIME:~3,2%%INPUT_TIME:~0,2%%INPUT_TIME:~11,2%%INPUT_TIME:~14,2%%INPUT_TIME:~17,2%"
set "OUTPUT_TIME=%OUTPUT_TIME:~6,4%%OUTPUT_TIME:~3,2%%OUTPUT_TIME:~0,2%%OUTPUT_TIME:~11,2%%OUTPUT_TIME:~14,2%%OUTPUT_TIME:~17,2%"

:: Compare timestamps
if "%INPUT_TIME%" GEQ "%OUTPUT_TIME%" (
    echo [%INPUT%] is newer than [%OUTPUT%]. Compiling...
    %SLANGC_CMD%
) else (
    echo [%OUTPUT%] is up to date. Skipping [%INPUT%].
)

goto :eof
