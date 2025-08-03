//
// Created by sansm on 8/2/2025.
//
#include <stdio.h>
#include <math.h>    // Without this the fabs returns wrong results
#include <string>
#include <vector>

#ifndef PROPRESULTS_H
#define PROPRESULTS_H

struct TimeStepData {
    double mse;                 // Mean solar ecliptic time
    double pos[3];             // Position (km)
    double vel[3];             // Velocity (km/s)
    double llh[3];             // Latitude(deg), Longitude(deg), Height above Geoid (km)
    double meanKep[6];         // Mean Keplerian elements
    double oscKep[6];          // Osculating Keplerian elements
    double nodalApPer[3];      // Nodal period, apogee, perigee
    double meanMotion;         // Mean motion
    std::string errorMsg;      // Error message if any
    bool hasError;             // Flag indicating if this timestep has an error
};

struct SatelliteData {
    std::string line1;                    // TLE line 1
    std::string line2;                    // TLE line 2
    __int64 satKey;                       // Satellite key
    std::vector<TimeStepData> timeSteps;  // All timestep data for this satellite
    bool propagationSuccess;              // Overall success flag for this satellite
};

struct PropagationResults {
    std::vector<SatelliteData> satellites;
    int totalSatellites;
    bool overallSuccess;
    std::string generalError;
};

#endif //PROPRESULTS_H
