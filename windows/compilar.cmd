@echo off
rem Compila AutoPrision2026.exe con el compilador de C# que ya trae Windows (.NET Framework 4).
cd /d "%~dp0"
"%WINDIR%\Microsoft.NET\Framework64\v4.0.30319\csc.exe" /nologo /codepage:65001 /optimize+ /target:winexe /platform:anycpu /out:AutoPrision2026.exe /r:System.Windows.Forms.dll /r:System.Drawing.dll AutoPrision.cs
if errorlevel 1 pause
