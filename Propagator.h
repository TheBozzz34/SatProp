//
// Created by sansm on 8/2/2025.
//

#ifndef PROPAGATOR_H
#define PROPAGATOR_H

#include <stdio.h>
#include "PropResults.h"

namespace SGP_IMPL {

    // File type constants for PrintHeader function
    const int FT_OSC_STATE = 0;
    const int FT_OSC_ELEM = 1;
    const int FT_MEAN_ELEM = 2;
    const int FT_LLH_ELEM = 3;
    const int FT_NODAL_AP_PER = 4;

    class Propagator {
    public:
        // Constructor
        Propagator();

        // Destructor
        ~Propagator();

        // Main SGP4 propagation function
        static PropagationResults RunOneSgp4Job(char* inFile, double startTime, double stopTime, double stepSize);

        // Print header function
        static void PrintHeader(FILE* fp, int fileType);

    private:
        // Helper functions that you'll likely need based on the implementation
        static void CalcStartStopTime(double epochDs50UTC, double* startTime,
                              double* stopTime, double* stepSize);

        static void CalcStartStopTimeFromParams(double epoch, double *tStart, double *tStop, double *tStep, double inputStart,
                                                double inputStop, double inputStep);

        static void PrintPosVel(FILE* fp, double mse, double pos[3], double vel[3]);

        static void PrintLLH(FILE* fp, double mse, double llh[3], double pos[3]);

        static void PrintOscEls(FILE* fp, double mse, double oscKep[6]);

        static void PrintMeanEls(FILE* fp, double mse, double meanKep[6]);

        static void PrintNodalApPer(FILE* fp, double mse, double meanMotion, double nodalApPer[3]);
    };

} // SGP_IMPL

#endif //PROPAGATOR_H