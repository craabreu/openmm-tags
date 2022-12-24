/* -------------------------------------------------------------------------- *
 *                          OpenMM Nonbonded Slicing                          *
 *                          ========================                          *
 *                                                                            *
 * An OpenMM plugin for slicing nonbonded potential calculations on the basis *
 * of atom pairs and for applying scaling parameters to selected slices.      *
 *                                                                            *
 * Copyright (c) 2022 Charlles Abreu                                          *
 * https://github.com/craabreu/openmm-nonbonded-slicing                       *
 * -------------------------------------------------------------------------- */

#ifdef WIN32
  #define _USE_MATH_DEFINES // Needed to get M_PI
#endif
#include "openmm/reference/ReferencePlatform.h"

extern "C" OPENMM_EXPORT void registerPmeSlicingReferenceKernelFactories();

OpenMM::ReferencePlatform platform;

void initializeTests(int argc, char* argv[]) {
    registerPmeSlicingReferenceKernelFactories();
    platform = dynamic_cast<OpenMM::ReferencePlatform&>(OpenMM::Platform::getPlatformByName("Reference"));
    if (argc > 1)
        platform.setPropertyDefaultValue("Precision", std::string(argv[1]));
    if (argc > 2)
        platform.setPropertyDefaultValue("DeviceIndex", std::string(argv[2]));
}