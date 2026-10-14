@echo off
rem Use the same encoding in Visual Studio and command-line Ninja builds.
chcp 65001 >nul
%*
exit /b %errorlevel%
