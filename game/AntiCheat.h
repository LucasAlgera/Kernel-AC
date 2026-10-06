#pragma once
#include "Windows.h"

typedef bool (*MouseClicked)();

void NotifyMouspress()
{
	HMODULE hModule = LoadLibraryA("dllservice.dll");
	if (!hModule)
	{
		//std::cout << "dllservice.dll not found!";
		return;
	}

	MouseClicked mouseClickedFunc = (MouseClicked)GetProcAddress(hModule, "NotifyMouseClicked");
	if (!mouseClickedFunc)
	{
		//std::cout << "NotifyMouseClicked not found!";
		return;
	}

	if (mouseClickedFunc())
	{
		std::cout << "notification successful!";
		FreeLibrary(hModule);


		return;
	}
	//std::cout << "notification unsuccessful!";
	FreeLibrary(hModule);
	return;
}