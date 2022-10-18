#ifndef __OPENMM_OPENCLVKFFT3D_H__
#define __OPENMM_OPENCLVKFFT3D_H__

/* -------------------------------------------------------------------------- *
 *                                   OpenMM                                   *
 * -------------------------------------------------------------------------- *
 * This is part of the OpenMM molecular simulation toolkit originating from   *
 * Simbios, the NIH National Center for Physics-Based Simulation of           *
 * Biological Structures at Stanford, funded under the NIH Roadmap for        *
 * Medical Research, grant U54 GM072970. See https://simtk.org.               *
 *                                                                            *
 * Portions copyright (c) 2009-2015 Stanford University and the Authors.      *
 * Authors: Peter Eastman                                                     *
 * Contributors:                                                              *
 *                                                                            *
 * This program is free software: you can redistribute it and/or modify       *
 * it under the terms of the GNU Lesser General Public License as published   *
 * by the Free Software Foundation, either version 3 of the License, or       *
 * (at your option) any later version.                                        *
 *                                                                            *
 * This program is distributed in the hope that it will be useful,            *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of             *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the              *
 * GNU Lesser General Public License for more details.                        *
 *                                                                            *
 * You should have received a copy of the GNU Lesser General Public License   *
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.      *
 * -------------------------------------------------------------------------- */

#include "openmm/opencl/OpenCLArray.h"
#define VKFFT_BACKEND 3 // OpenCL
#include "internal/vkFFT.h"

using namespace OpenMM;

namespace PmeSlicing {

/**
 * This class performs three dimensional Fast Fourier Transforms using VkFFT by
 * Dmitrii Tolmachev (https://github.com/DTolm/VkFFT).
 *
 * Note that this class performs an unnormalized transform.  That means that if you perform
 * a forward transform followed immediately by an inverse transform, the effect is to
 * multiply every value of the original data set by the total number of data points.
 */

class OpenCLVkFFT3D {
public:
    /**
     * Create an OpenCLVkFFT3D object for performing transforms of a particular size.
     *
     * The transform cannot be done in-place: the input and output
     * arrays must be different.  Also, the input array is used as workspace, so its contents
     * are destroyed.  This also means that both arrays must be large enough to hold complex values,
     * even when performing a real-to-complex transform.
     *
     * When performing a real-to-complex transform, the output data is of size xsize*ysize*(zsize/2+1)
     * and contains only the non-redundant elements.
     *
     * @param context the context in which to perform calculations
     * @param xsize   the first dimension of the data sets on which FFTs will be performed
     * @param ysize   the second dimension of the data sets on which FFTs will be performed
     * @param zsize   the third dimension of the data sets on which FFTs will be performed
     * @param batch   the number of FFTs
     * @param realToComplex  if true, a real-to-complex transform will be done.  Otherwise, it is complex-to-complex.
     * @param in      the data to transform, ordered such that in[x*ysize*zsize + y*zsize + z] contains element (x, y, z)
     * @param out     on exit, this contains the transformed data
     */
    OpenCLVkFFT3D(OpenCLContext& context, int xsize, int ysize, int zsize, int batch, bool realToComplex, OpenCLArray& in, OpenCLArray& out);
    ~OpenCLVkFFT3D();
    /**
     * Perform a Fourier transform.
     *
     * @param forward  true to perform a forward transform, false to perform an inverse transform
     * @param commandQueue   the OpenCL command queue doing the calculations
     */
    void execFFT(bool forward, cl::CommandQueue queue);
    /**
     * Get the smallest legal size for a dimension of the grid (that is, a size with no prime
     * factors other than 2, 3, 5, ..., maxPrimeFactor).
     *
     * @param minimum   the minimum size the return value must be greater than or equal to
     * @param maxPrimeFactor  the maximum supported prime number factor (default=7)
     */
    static int findLegalDimension(int minimum, int maxPrimeFactor=7) {  // VkFFT allows maxPrimeFactor up to 13
        if (minimum < 1)
            return 1;
        while (true) {
            // Attempt to factor the current value.

            int unfactored = minimum;
            for (int factor = 2; factor <= maxPrimeFactor; factor++) {
                while (unfactored > 1 && unfactored%factor == 0)
                    unfactored /= factor;
            }
            if (unfactored == 1)
                return minimum;
            minimum++;
        }
    }
private:
    cl_mem inputBuffer;
    cl_mem outputBuffer;
    cl_device_id device;
    cl_context cl;
    uint64_t inputBufferSize;
    uint64_t outputBufferSize;
    VkFFTApplication app = {};
};

} // namespace PmeSlicing

#endif // __OPENMM_OPENCLVKFFT3D_H__