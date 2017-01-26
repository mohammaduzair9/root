// @(#)root/meta:$Id$

/*************************************************************************
 * Copyright (C) 1995-2016, Rene Brun and Fons Rademakers.               *
 * All rights reserved.                                                  *
 *                                                                       *
 * For the licensing terms see $ROOTSYS/LICENSE.                         *
 * For the list of contributors see $ROOTSYS/README/CREDITS.             *
 *************************************************************************/

#ifndef R__DLLEXPORT
# if _WIN32
#  define R__DLLEXPORT __declspec(dllexport)
# else
#  define R__DLLEXPORT __attribute__ ((visibility ("default")))
# endif
#endif

#include "rootclingTCling.h"

#undef R__DLLEXPORT

#include "TROOT.h"
#include "TCling.h"

extern "C"
const char ** *TROOT__GetExtraInterpreterArgs()
{
  return &TROOT::GetExtraInterpreterArgs();
}

extern "C"
cling::Interpreter *TCling__GetInterpreter()
{
  static bool isInitialized = false;
  gROOT; // trigger initialization
  if (!isInitialized) {
    gInterpreter->SetClassAutoloading(false);
    isInitialized = true;
  }
  return (cling::Interpreter*) ((TCling*)gCling)->GetInterpreterImpl();
}

