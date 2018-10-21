
#include "stdafx.h"

#include "KKTask.h"

DWORD KKTaskable::async_(void *param) {
	((KKTaskable *)param)->run();
	return 0;
}
