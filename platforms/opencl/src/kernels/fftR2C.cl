/**
 * Combine the two halves of a real grid into a complex grid that is half as large.
 */
__kernel void packForwardData(__global const real* restrict in, __global real2* restrict out) {
    const int gridSize = PACKED_XSIZE*PACKED_YSIZE*PACKED_ZSIZE;
    for (int offset = 0; offset < ALL_GRIDS; offset += ONE_GRID) {
        for (int index = get_global_id(0); index < gridSize; index += get_global_size(0)) {
            int x = index/(PACKED_YSIZE*PACKED_ZSIZE);
            int remainder = index-x*(PACKED_YSIZE*PACKED_ZSIZE);
            int y = remainder/PACKED_ZSIZE;
            int z = remainder-y*PACKED_ZSIZE;
#if PACKED_AXIS == 0
            real2 value = (real2) (in[offset+2*x*YSIZE*ZSIZE+y*ZSIZE+z], in[offset+(2*x+1)*YSIZE*ZSIZE+y*ZSIZE+z]);
#elif PACKED_AXIS == 1
            real2 value = (real2) (in[offset+x*YSIZE*ZSIZE+2*y*ZSIZE+z], in[offset+x*YSIZE*ZSIZE+(2*y+1)*ZSIZE+z]);
#else
            real2 value = (real2) (in[offset+x*YSIZE*ZSIZE+y*ZSIZE+2*z], in[offset+x*YSIZE*ZSIZE+y*ZSIZE+(2*z+1)]);
#endif
            out[offset+index] = value;
        }
    }
}

/**
 * Split the transformed data back into a full sized, symmetric grid.
 */
__kernel void unpackForwardData(__global const real2* restrict in, __global real2* restrict out, __local real2* restrict w) {
    for (int offset = 0; offset < ALL_GRIDS; offset += ONE_GRID) {
        // Compute the phase factors.
    
#if PACKED_AXIS == 0
        for (int i = get_local_id(0); i < PACKED_XSIZE; i += get_local_size(0))
            w[i] = (real2) (sin(i*2*M_PI/XSIZE), cos(i*2*M_PI/XSIZE));
#elif PACKED_AXIS == 1
        for (int i = get_local_id(0); i < PACKED_YSIZE; i += get_local_size(0))
            w[i] = (real2) (sin(i*2*M_PI/YSIZE), cos(i*2*M_PI/YSIZE));
#else
        for (int i = get_local_id(0); i < PACKED_ZSIZE; i += get_local_size(0))
            w[i] = (real2) (sin(i*2*M_PI/ZSIZE), cos(i*2*M_PI/ZSIZE));
#endif
        barrier(CLK_LOCAL_MEM_FENCE);

        // Transform the data.
    
        const int gridSize = PACKED_XSIZE*PACKED_YSIZE*PACKED_ZSIZE;
        const int outputZSize = ZSIZE/2+1;
        for (int index = get_global_id(0); index < gridSize; index += get_global_size(0)) {
            int x = index/(PACKED_YSIZE*PACKED_ZSIZE);
            int remainder = index-x*(PACKED_YSIZE*PACKED_ZSIZE);
            int y = remainder/PACKED_ZSIZE;
            int z = remainder-y*PACKED_ZSIZE;
            int xp = (x == 0 ? 0 : PACKED_XSIZE-x);
            int yp = (y == 0 ? 0 : PACKED_YSIZE-y);
            int zp = (z == 0 ? 0 : PACKED_ZSIZE-z);
            real2 z1 = in[offset+x*PACKED_YSIZE*PACKED_ZSIZE+y*PACKED_ZSIZE+z];
            real2 z2 = in[offset+xp*PACKED_YSIZE*PACKED_ZSIZE+yp*PACKED_ZSIZE+zp];
#if PACKED_AXIS == 0
            real2 wfac = w[x];
#elif PACKED_AXIS == 1
            real2 wfac = w[y];
#else
            real2 wfac = w[z];
#endif
            real2 output = (real2) ((z1.x+z2.x - wfac.x*(z1.x-z2.x) + wfac.y*(z1.y+z2.y))/2, (z1.y-z2.y - wfac.y*(z1.x-z2.x) - wfac.x*(z1.y+z2.y))/2);
            if (z < outputZSize)
                out[offset+x*YSIZE*outputZSize+y*outputZSize+z] = output;
            xp = (x == 0 ? 0 : XSIZE-x);
            yp = (y == 0 ? 0 : YSIZE-y);
            zp = (z == 0 ? 0 : ZSIZE-z);
            if (zp < outputZSize) {
#if PACKED_AXIS == 0
                if (x == 0)
                    out[offset+PACKED_XSIZE*YSIZE*outputZSize+yp*outputZSize+zp] = (real2) ((z1.x-z1.y+z2.x-z2.y)/2, (-z1.x-z1.y+z2.x+z2.y)/2);
#elif PACKED_AXIS == 1
                if (y == 0)
                    out[offset+xp*YSIZE*outputZSize+PACKED_YSIZE*outputZSize+zp] = (real2) ((z1.x-z1.y+z2.x-z2.y)/2, (-z1.x-z1.y+z2.x+z2.y)/2);
#else
                if (z == 0)
                    out[offset+xp*YSIZE*outputZSize+yp*outputZSize+PACKED_ZSIZE] = (real2) ((z1.x-z1.y+z2.x-z2.y)/2, (-z1.x-z1.y+z2.x+z2.y)/2);
#endif
                else
                    out[offset+xp*YSIZE*outputZSize+yp*outputZSize+zp] = (real2) (output.x, -output.y);
            }
        }
    }
}

/**
 * Load a value from the half-complex grid produced by a real-to-complex transform.
 */
real2 loadComplexValue(__global const real2* restrict in, int x, int y, int z, int offset) {
    const int inputZSize = ZSIZE/2+1;
    if (z < inputZSize)
        return in[offset+offset+x*YSIZE*inputZSize+y*inputZSize+z];
    int xp = (x == 0 ? 0 : XSIZE-x);
    int yp = (y == 0 ? 0 : YSIZE-y);
    real2 value = in[offset+offset+xp*YSIZE*inputZSize+yp*inputZSize+(ZSIZE-z)];
    return (real2) (value.x, -value.y);
}

/**
 * Repack the symmetric complex grid into one half as large in preparation for doing an inverse complex-to-real transform.
 */
__kernel void packBackwardData(__global const real2* restrict in, __global real2* restrict out, __local real2* restrict w) {
    for (int offset = 0; offset < ALL_GRIDS; offset += ONE_GRID) {
        // Compute the phase factors.
    
#if PACKED_AXIS == 0
        for (int i = get_local_id(0); i < PACKED_XSIZE; i += get_local_size(0))
            w[i] = (real2) (cos(i*2*M_PI/XSIZE), sin(i*2*M_PI/XSIZE));
#elif PACKED_AXIS == 1
        for (int i = get_local_id(0); i < PACKED_YSIZE; i += get_local_size(0))
            w[i] = (real2) (cos(i*2*M_PI/YSIZE), sin(i*2*M_PI/YSIZE));
#else
        for (int i = get_local_id(0); i < PACKED_ZSIZE; i += get_local_size(0))
            w[i] = (real2) (cos(i*2*M_PI/ZSIZE), sin(i*2*M_PI/ZSIZE));
#endif
        barrier(CLK_LOCAL_MEM_FENCE);

        // Transform the data.
        
        const int gridSize = PACKED_XSIZE*PACKED_YSIZE*PACKED_ZSIZE;
        for (int index = get_global_id(0); index < gridSize; index += get_global_size(0)) {
            int x = index/(PACKED_YSIZE*PACKED_ZSIZE);
            int remainder = index-x*(PACKED_YSIZE*PACKED_ZSIZE);
            int y = remainder/PACKED_ZSIZE;
            int z = remainder-y*PACKED_ZSIZE;
            int xp = (x == 0 ? 0 : PACKED_XSIZE-x);
            int yp = (y == 0 ? 0 : PACKED_YSIZE-y);
            int zp = (z == 0 ? 0 : PACKED_ZSIZE-z);
            real2 z1 = loadComplexValue(in, x, y, z, offset);
#if PACKED_AXIS == 0
            real2 wfac = w[x];
            real2 z2 = loadComplexValue(in, PACKED_XSIZE-x, yp, zp, offset);
#elif PACKED_AXIS == 1
            real2 wfac = w[y];
            real2 z2 = loadComplexValue(in, xp, PACKED_YSIZE-y, zp, offset);
#else
            real2 wfac = w[z];
            real2 z2 = loadComplexValue(in, xp, yp, PACKED_ZSIZE-z, offset);
#endif
            real2 even = (real2) ((z1.x+z2.x)/2, (z1.y-z2.y)/2);
            real2 odd = (real2) ((z1.x-z2.x)/2, (z1.y+z2.y)/2);
            odd = (real2) (odd.x*wfac.x-odd.y*wfac.y, odd.y*wfac.x+odd.x*wfac.y);
            out[offset+x*PACKED_YSIZE*PACKED_ZSIZE+y*PACKED_ZSIZE+z] = (real2) (even.x-odd.y, even.y+odd.x);
        }
    }
}

/**
 * Split the data back into a full sized, real grid after an inverse transform.
 */
__kernel void unpackBackwardData(__global const real2* restrict in, __global real* restrict out) {
    const int gridSize = PACKED_XSIZE*PACKED_YSIZE*PACKED_ZSIZE;
    for (int offset = 0; offset < ALL_GRIDS; offset += ONE_GRID) {
        for (int index = get_global_id(0); index < gridSize; index += get_global_size(0)) {
            int x = index/(PACKED_YSIZE*PACKED_ZSIZE);
            int remainder = index-x*(PACKED_YSIZE*PACKED_ZSIZE);
            int y = remainder/PACKED_ZSIZE;
            int z = remainder-y*PACKED_ZSIZE;
            real2 value = 2*in[offset+index];
#if PACKED_AXIS == 0
            out[offset+2*x*YSIZE*ZSIZE+y*ZSIZE+z] = value.x;
            out[offset+(2*x+1)*YSIZE*ZSIZE+y*ZSIZE+z] = value.y;
#elif PACKED_AXIS == 1
            out[offset+x*YSIZE*ZSIZE+2*y*ZSIZE+z] = value.x;
            out[offset+x*YSIZE*ZSIZE+(2*y+1)*ZSIZE+z] = value.y;
#else
            out[offset+x*YSIZE*ZSIZE+y*ZSIZE+2*z] = value.x;
            out[offset+x*YSIZE*ZSIZE+y*ZSIZE+(2*z+1)] = value.y;
#endif
        }
    }
}
