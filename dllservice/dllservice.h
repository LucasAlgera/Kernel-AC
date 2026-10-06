#pragma once

#ifdef DLLSERVICE_EXPORTS
#define DLLSERVICE_API __declspec(dllexport)
#else
#define DLLSERVICE_API __declspec(dllimport)
#endif

extern "C" DLLSERVICE_API bool NotifyMouseClicked(void);