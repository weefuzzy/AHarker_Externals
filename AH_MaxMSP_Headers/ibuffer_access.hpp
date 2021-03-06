
/*
 *  ibuffer_access.h
 *
 *	This header file provides an interface for accessing and interpolating samples from an ibuffer (or standard MSP buffer).
 *	You should also compile ibufffer_access.cpp in the project
 *
 *	Various kinds of interpolation are supported along with both SIMD calculation and both 32 bit and 64 bit floating point precision.
 *
 *	All pointers used with the SIMD routines should be 16-byte aligned and a multiple of 4 values wide (regardless of type).
 *  The SIMD routines currently always write a multiple of 4 values, although any extra values will be zero.
 *	Pointers for non-SIMD routines can be arbitrarily aligned.
 *
 *	The init routine should be called in an objects main function to setup the necessary variables.
 *	Use the ibuffer__get_ptr / ibuffer_info / ibuffer_sample_rate functions to get info about the ibuffer (or buffer).
 *	The buffer inuse field should be incremented and decremented beoore and after use using ibuffer_increment_inuse and ibuffer_decrement_inuse.
 *
 *	The main routines for accessing samples are then:
 *
 *	ibuffer_get_samp							- get a single sample
 *	ibuffer_get_samps							- get a number of consecutive samples in forward or reverse order
 *
 *	offsets is a pointer to the sample offsets
 *	fracts is a pointer to the subsample (fractional) offsets
 *	n_samps is the number of samples required.
 *	chan is the channel you wish to access.
 *
 *	Most of these routines have a mul input to multiply by a constant value.
 *	Other variables should be self-explanatory 
 *	
 *  Copyright 2010-13 Alex Harker. All rights reserved.
 *
 */


#ifndef _IBUFFER_ACCESS_
#define _IBUFFER_ACCESS_

#include "ibuffer.h"
#include <TableReader.hpp>

enum BufferType { kBufferNone, kBufferIBuffer, kBufferMaxBuffer };

class ibuffer_data
{
    static t_symbol * ps_buffer;
    static t_symbol *ps_ibuffer;
    
public:
    
    ibuffer_data(t_symbol *name);
    ~ibuffer_data();
    
    void set_dirty();
    void set_size_in_samples(t_atom_long size);
    
    void release();
    
    BufferType get_type() const         { return buffer_type; };
    void *get_samples() const           { return samples; };
    t_ptr_int get_length() const        { return length; }
    t_ptr_int get_num_chans() const     { return num_chans; }
    double get_sample_rate() const      { return sample_rate; }
    long get_format() const             { return format; }
    
private:
    
    void release_buffer();

    BufferType buffer_type;

    void *samples;
    
    t_ptr_int length;
    long num_chans;
    long format;
    
    double sample_rate;
 
    t_object *buffer_object;
};

// Reading different formats

template <class T, int64_t bit_scale> struct fetch : public table_fetcher<float>
{
    fetch(const ibuffer_data& data, long chan)
    : table_fetcher(1.0 / ((int64_t) 1 << (bit_scale - 1))), samples(((T *) data.get_samples()) + chan), num_chans(data.get_num_chans()) {}
    
    T operator()(intptr_t offset)   { return samples[offset * num_chans]; }
    double get(intptr_t offset)     { return bit_scale != 1 ? scale * operator()(offset) : operator()(offset); }
    
    T *samples;
    long num_chans;
};

template<> struct fetch<int32_t, 24> : public table_fetcher<float>
{
    fetch(const ibuffer_data& data, long chan)
    : table_fetcher(1.0 / ((int64_t) 1 << 31)), samples(((uint8_t *) data.get_samples()) + 3 * chan), num_chans(data.get_num_chans()) {}
    
    int32_t operator()(intptr_t offset)
    {
        return (*reinterpret_cast<uint32_t *>(samples + (offset * 3 * num_chans - 1)) & 0xFFFFFF00);
    }
    double get(intptr_t offset) { return scale * operator()(offset); }
    
    uint8_t *samples;
    long num_chans;
};

typedef fetch<float, 1> fetch_float;
typedef fetch<int16_t, 16> fetch_16bit;
typedef fetch<int32_t, 24> fetch_24bit;
typedef fetch<int32_t, 32> fetch_32bit;

// Get the value of an individual sample

static inline double ibuffer_get_samp(const ibuffer_data& buffer, intptr_t offset, long chan);

// Get consecutive samples (and in reverse)

void ibuffer_get_samps(const ibuffer_data& buffer, float *out, intptr_t offset, intptr_t n_samps, long chan, bool reverse = false);
void ibuffer_get_samps(const ibuffer_data& buffer, double *out, intptr_t offset, intptr_t n_samps, long chan, bool reverse = false);

// Read with various forms of interpolation

void ibuffer_read(const ibuffer_data& buffer, float *out, double *positions, intptr_t n_samps, long chan, float mul, InterpType interp);
void ibuffer_read(const ibuffer_data& buffer, double *out, double *positions, intptr_t n_samps, long chan, double mul, InterpType interp);
void ibuffer_read(const ibuffer_data& buffer, float *out, float *positions, intptr_t n_samps, long chan, float mul, InterpType interp);

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////// Get individual samples /////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static inline double ibuffer_get_samp(const ibuffer_data& buffer, intptr_t offset, long chan)
{
    switch (buffer.get_format())
    {
        case PCM_FLOAT:     return fetch_float(buffer, chan).get(offset);
        case PCM_INT_16:    return fetch_16bit(buffer, chan).get(offset);
        case PCM_INT_24:    return fetch_24bit(buffer, chan).get(offset);
        case PCM_INT_32:    return fetch_32bit(buffer, chan).get(offset);
    }

    return 0.0;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////// Interpolation Attributes ////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template <class T, InterpType defaultValue>
t_max_err ibuf_interp_attribute_set(T *x, t_attr *a, long argc, t_atom *argv)
{
    if (!argc)
    {
        x->interp_type = defaultValue;
        return MAX_ERR_NONE;
    }
    
    if (atom_gettype(argv) == A_SYM)
    {
        t_symbol *type = atom_getsym(argv);
        
        if (type == gensym("linear"))
            x->interp_type = kInterpLinear;
        else if (type == gensym("hermite"))
            x->interp_type = kInterpCubicHermite;
        else if (type == gensym("bspline"))
            x->interp_type = kInterpCubicBSpline;
        else if (type == gensym("lagrange"))
            x->interp_type = kInterpCubicLagrange;
        else
            object_error((t_object *) x, "%s: no interpolation mode %s", object_classname(x)->s_name,  type->s_name);
    }
    else
    {
        t_atom_long index = atom_getlong(argv) - 1;
        index = index < 0 ? 0 : index;
        index = index > 3 ? 3 : index;
        
        x->interp_type = static_cast<InterpType>(index);
    }
    
    return MAX_ERR_NONE;
}

template <class T>
t_max_err ibuf_interp_attribute_get(T *x, t_object *attr, long *argc, t_atom **argv)
{
    if (argc && argv)
    {
        char alloc;
        
        if (atom_alloc(argc, argv, &alloc))
            return MAX_ERR_GENERIC;
        
        switch (x->interp_type)
        {
            case kInterpLinear:             atom_setsym(*argv, gensym("linear"));       break;
            case kInterpCubicHermite:       atom_setsym(*argv, gensym("hermite"));      break;
            case kInterpCubicBSpline:       atom_setsym(*argv, gensym("bspline"));      break;
            case kInterpCubicLagrange:      atom_setsym(*argv, gensym("lagrange"));     break;
            
            default:
                atom_setsym(*argv, gensym("linear"));
        }
    }
    
    return MAX_ERR_NONE;
}

template <class T, InterpType defaultValue>
void add_ibuffer_interp_attribute(t_class *this_class, const char *attrname)
{
    CLASS_ATTR_LONG(this_class, attrname, 0L, T, interp_type);
    CLASS_ATTR_ENUM(this_class, attrname, 0L, "linear hermite bspline lagrange");
    CLASS_ATTR_ACCESSORS(this_class, attrname, ibuf_interp_attribute_get<T>, (ibuf_interp_attribute_set<T, defaultValue>));
    CLASS_ATTR_LABEL(this_class, attrname, 0L, "Interpolation Type");
}

#endif	/* _IBUFFER_ACCESS_ */
