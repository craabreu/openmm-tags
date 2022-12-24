#ifndef OPENMM_SLICEDNONBONDEDFORCE_PROXY_H_
#define OPENMM_SLICEDNONBONDEDFORCE_PROXY_H_

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

#include "internal/windowsExportPmeSlicing.h"
#include "openmm/serialization/SerializationProxy.h"

using namespace OpenMM;

namespace NonbondedSlicing {

/**
 * This is a proxy for serializing SlicedNonbondedForce objects.
 */

class OPENMM_EXPORT_PMESLICING SlicedNonbondedForceProxy : public SerializationProxy {
public:
    SlicedNonbondedForceProxy();
    void serialize(const void* object, SerializationNode& node) const;
    void* deserialize(const SerializationNode& node) const;
};

} // namespace OpenMM

#endif /*OPENMM_SLICEDNONBONDEDFORCE_PROXY_H_*/
