@echo off

pushd ..\build
cl -Zi E:\CodeProjects\C\MyFirstGame\ShowcaseVersion\code\GameTemplate.cpp user32.lib gdi32.lib
popd