#ifdef DLLSERVICE_EXPORTS
#define DLLSERVICE_API __declspec(dllexport)
#else
#define DLLSERVICE_API __declspec(dllimport)
#endif

DLLSERVICE_API bool NotifyMouseClicked(void);
