#ifndef OPENMM_OPENCLPARALLELPMESLICINGKERNELS_H_
#define OPENMM_OPENCLPARALLELPMESLICINGKERNELS_H_

/* -------------------------------------------------------------------------- *
 *                             OpenMM PME Slicing                             *
 *                             ==================                             *
 *                                                                            *
 * An OpenMM plugin for Smooth Particle Mesh Ewald electrostatic calculations *
 * with multiple coupling parameters.                                         *
 *                                                                            *
 * Copyright (c) 2022 Charlles Abreu                                          *
 * https://github.com/craabreu/openmm-pme-slicing                             *
 * -------------------------------------------------------------------------- */

#include "OpenCLPmeSlicingKernels.h"
#include "CommonPmeSlicingKernels.h"
#include "openmm/opencl/OpenCLPlatform.h"
#include "openmm/opencl/OpenCLContext.h"

namespace PmeSlicing {

/**
 * This kernel is invoked by SlicedPmeForce to calculate the forces acting on the system.
 */
class OpenCLParallelCalcSlicedPmeForceKernel : public CalcSlicedPmeForceKernel {
public:
    OpenCLParallelCalcSlicedPmeForceKernel(std::string name, const Platform& platform, OpenCLPlatform::PlatformData& data, const System& system);
    OpenCLCalcSlicedPmeForceKernel& getKernel(int index) {
        return dynamic_cast<OpenCLCalcSlicedPmeForceKernel&>(kernels[index].getImpl());
    }
    /**
     * Initialize the kernel.
     *
     * @param system     the System this kernel will be applied to
     * @param force      the SlicedPmeForce this kernel will be used for
     */
    void initialize(const System& system, const SlicedPmeForce& force);
    /**
     * Execute the kernel to calculate the forces and/or energy.
     *
     * @param context        the context in which to execute this kernel
     * @param includeForces  true if forces should be calculated
     * @param includeEnergy  true if the energy should be calculated
     * @param includeReciprocal  true if reciprocal space interactions should be included
     * @param includeReciprocal  true if reciprocal space interactions should be included
     * @return the potential energy due to the force
     */
    double execute(ContextImpl& context, bool includeForces, bool includeEnergy, bool includeDirect, bool includeReciprocal);
    /**
     * Copy changed parameters over to a context.
     *
     * @param context    the context to copy parameters to
     * @param force      the SlicedPmeForce to copy the parameters from
     */
    void copyParametersToContext(ContextImpl& context, const SlicedPmeForce& force);
    /**
     * Get the parameters being used for PME.
     *
     * @param alpha   the separation parameter
     * @param nx      the number of grid points along the X axis
     * @param ny      the number of grid points along the Y axis
     * @param nz      the number of grid points along the Z axis
     */
    void getPMEParameters(double& alpha, int& nx, int& ny, int& nz) const;
private:
    class Task;
    OpenCLPlatform::PlatformData& data;
    std::vector<Kernel> kernels;
};

} // namespace OpenMM

#endif /*OPENMM_OPENCLPARALLELPMESLICINGKERNELS_H_*/
