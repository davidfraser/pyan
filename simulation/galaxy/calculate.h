#ifndef CALCULATE_H
#define CALCULATE_H

#include "galaxy.h"

typedef struct CALC_DATA CALC_DATA;

/** A generic calculator. */
typedef struct CALCULATOR {
    /** Gravitational constant. */
    double gravity;
    
    /** Private data for use by implementation. */
    CALC_DATA *data;
    
    /** Calculate the forces. */
    void (* calculate)(struct CALCULATOR *calculator, GALAXY *g, VECTOR *forces);
    
    /** Destroy the calculator. */
    void (* destroy)(struct CALCULATOR *calculator);
} CALCULATOR;

extern void calculate__calculate_force(STAR *s1, STAR *s2, double g, VECTOR force);
extern void calculate__apply_forces(GALAXY *g, VECTOR *forces, double timestep);
extern CALCULATOR *calculate__naive_calculator(void);

static const struct {
    /** Calculate the force between two bodies. */
    void (* calculate_force)(STAR *s1, STAR *s2, double g, VECTOR force);
    
    /** Apply the forces in forces to the stars in galaxy, over the given timestep. */
    void (* apply_forces)(GALAXY *galaxy, VECTOR *forces, double timestep);
    
    /** Construct a calculator that uses a naive n^2 algorithm. */
    CALCULATOR *(* naive_calculator)(void);
} calculate = {
    calculate__calculate_force,
    calculate__apply_forces,
    calculate__naive_calculator
};

#endif
