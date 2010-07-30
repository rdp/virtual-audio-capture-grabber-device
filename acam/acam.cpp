// acam.cpp : Defines the exported functions for the DLL application.
//

#include "stdafx.h"
#include "acam.h"


// This is an example of an exported variable
ACAM_API int nacam=0;

// This is an example of an exported function.
ACAM_API int fnacam(void)
{
	return 42;
}

// This is the constructor of a class that has been exported.
// see acam.h for the class definition
Cacam::Cacam()
{
	return;
}
