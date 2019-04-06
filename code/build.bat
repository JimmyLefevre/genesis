@echo off

REM call "..\..\tools\msvc.bat"

set ProjectName=genesis
set DevFlag=1
set BuildAssetsOnStartupFlag=1

set CommonCompilerFlags= -Od -nologo -fp:fast -fp:except- -Gm- -GR- -EHa- -Zo -Oi -WX -W4 -wd4189 -wd4201 -wd4505 -wd4127 -wd4100 -FC -Z7
REM Custom compiler flags
set CommonCompilerFlags=%CommonCompilerFlags% -DGENESIS_DEV=%DevFlag% -DGENESIS_BUILD_ASSETS_ON_STARTUP=%BuildAssetsOnStartupFlag%
REM set CommonCompilerFlags=-analyze %CommonCompilerFlags%

set CommonLinkerFlags= -incremental:no -opt:ref

set NoCRTCompilerFlags=-GS- -Gs9999999
set NoCRTLinkerFlags=-NODEFAULTLIB -subsystem:windows -STACK:0x100000,0x100000

IF NOT EXIST ..\..\build\%ProjectName% mkdir ..\..\build\%ProjectName%
pushd ..\..\build\%ProjectName%

del *.pdb > NUL 2> NUL

REM 32-bit build
REM cl %CommonCompilerFlags% ..\..\%ProjectName%\code\win_main.cpp /link -subsystem:windows,5.1 %CommonLinkerFlags%

REM 64-bit build
REM Optimization switches /wO2
cl %CommonCompilerFlags% ..\..\%ProjectName%\code\win_main.cpp %NoCRTCompilerFlags% -Fewin_%ProjectName%.exe -Fm /link %CommonLinkerFlags% %NoCRTLinkerFlags% kernel32.lib user32.lib gdi32.lib winmm.lib shlwapi.lib
cl %CommonCompilerFlags% -MTd ..\..\%ProjectName%\code\game.cpp -Fe%ProjectName% -Fd -LDd -Fm %NoCRTCompilerFlags% /link -PDB:%ProjectName%%random%.pdb %NoCRTLinkerFlags% %CommonLinkerFlags% -EXPORT:g_get_api
popd