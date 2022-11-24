#ifndef OPENMM_SLICEDNONBONDEDFORCE_H_
#define OPENMM_SLICEDNONBONDEDFORCE_H_

/* -------------------------------------------------------------------------- *
 *                             OpenMM PME Slicing                             *
 *                             ==================                             *
 *                                                                            *
 * An OpenMM plugin for slicing Particle Mesh Ewald calculations on the basis *
 * of atom pairs and applying a different switching parameter to each slice.  *
 *                                                                            *
 * Copyright (c) 2022 Charlles Abreu                                          *
 * https://github.com/craabreu/openmm-pme-slicing                             *
 * -------------------------------------------------------------------------- */

#include "openmm/NonbondedForce.h"
#include "internal/windowsExportPmeSlicing.h"
#include <map>

using namespace OpenMM;

namespace PmeSlicing {

class OPENMM_EXPORT_PMESLICING SlicedNonbondedForce : public NonbondedForce {
public:
    SlicedNonbondedForce(int numSubsets);
    int getNumSubsets() const {
        return numSubsets;
    }
    void setParticleSubset(int index, int subset);
    int getParticleSubset(int index) const;
protected:
    ForceImpl* createImpl() const;
private:
    int numSubsets;
    std::map<int, int> subsets;
};

} // namespace PmeSlicing

#endif /*OPENMM_SLICEDNONBONDEDFORCE_H_*/
