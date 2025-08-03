//
// Created by sansm on 8/2/2025.
//
#include <stdio.h>
#include <math.h>    // Without this the fabs returns wrong results
#include "Propagator.h"
#include "PropResults.h"

// C interface wrapper
#ifdef __cplusplus
extern "C"
{
#endif

#include "services/DllMainDll_Service.h"
#include "services/TimeFuncDll_Service.h"
#include "wrappers/DllMainDll.h"
#include "wrappers/EnvConstDll.h"
#include "wrappers/AstroFuncDll.h"
#include "wrappers/TimeFuncDll.h"
#include "wrappers/TleDll.h"
#include "wrappers/Sgp4PropDll.h"

#ifdef __cplusplus
}
#endif


namespace SGP_IMPL {
    Propagator::Propagator() = default;
    Propagator::~Propagator() = default;
    PropagationResults Propagator::RunOneSgp4Job(char* inFile)
{
    const double EPSI = 0.00050;	/*	TIME TOLERANCE IN SEC.	*/

    PropagationResults results;
    results.overallSuccess = true;
    results.totalSatellites = 0;

    double startTime;
    double stopTime;
    double stepSize;

    char  errMsg[LOGMSGLEN];
    char  valueStr[GETSETSTRLEN];
    char  line1[INPUTCARDLEN];
    char  line2[INPUTCARDLEN];

    int   errCode;
    int   numSats;
    int   i;
    int   step;
    int   order = 2;   // Get the satKeys in the order they were read

    double mse, ds50UTC, epochDs50UTC;
    double meanMotion;

    __int64* pSatKeys;

    // propagator output data
    double
        pos[3],           //Position (km)
        vel[3],           //Velocity (km/s)
        llh[3],           // Latitude(deg), Longitude(deg), Height above Geoid (km)
        meanKep[6],       //Mean Keplerian elements
        oscKep[6],        //Osculating Keplerian elements
        nodalApPer[3];    //Nodal period, apogee, perigee

    // Load all SGP4-related data in one call
    Sgp4LoadFileAll(inFile);

    // number of satellites currently loaded in memory
    numSats = TleGetCount();
    if (numSats == 0)
    {
        results.overallSuccess = false;
        results.generalError = "No TLEs were found in the input file";
        return results;
    }

    results.totalSatellites = numSats;

    //dynamic array alloc for satellite keys
    pSatKeys = (__int64*)malloc(numSats * sizeof(__int64));
    if (pSatKeys == nullptr)
    {
        results.overallSuccess = false;
        results.generalError = "Memory allocation failed for satellite keys";
        return results;
    }

    // get all the satellites ids from memory and store them in the local array
    TleGetLoaded(order, pSatKeys);

        //reserving space for the satellites in results
    results.satellites.reserve(numSats);

    // tle loop
    for (i = 0; i < numSats; i++)
    {
        SatelliteData satData;
        satData.satKey = pSatKeys[i];
        satData.propagationSuccess = true;

        // me when two line element set
        TleGetLines(pSatKeys[i], line1, line2);
        line1[INPUTCARDLEN - 1] = 0;
        line2[INPUTCARDLEN - 1] = 0;

        satData.line1 = std::string(line1);
        satData.line2 = std::string(line2);

        // init sat
        if (Sgp4InitSat(pSatKeys[i]) != 0)
        {
            satData.propagationSuccess = false;
            GetLastErrMsg(errMsg);
            errMsg[LOGMSGLEN - 1] = 0;

            TimeStepData errorStep;
            errorStep.hasError = true;
            errorStep.errorMsg = std::string(errMsg);
            satData.timeSteps.push_back(errorStep);

            results.satellites.push_back(satData);
            continue;
        }

        TleGetField(pSatKeys[i], XF_TLE_EPOCH, valueStr);
        valueStr[GETSETSTRLEN - 1] = 0;
        epochDs50UTC = DTGToUTC(valueStr); // Convert epoch string to days since 1950

        // compute start/stop times and step size from the input 6P card
        CalcStartStopTime(epochDs50UTC, &startTime, &stopTime, &stepSize);

        step = 0;
        ds50UTC = startTime;

        // Loop through all the time steps
        while (1)
        {
            if (stepSize >= 0 && ds50UTC >= stopTime)
                break;
            else if (stepSize < 0 && ds50UTC <= stopTime)
                break;

            ds50UTC = startTime + (step * stepSize / 1440.0);

            if ((stepSize >= 0 && ds50UTC + (EPSI / 86400) > stopTime) ||
                (stepSize < 0 && ds50UTC - (EPSI / 86400) < stopTime))
                ds50UTC = stopTime;

            // propagate satellite to the current time step
            errCode = Sgp4PropDs50UTC(pSatKeys[i], ds50UTC, &mse, pos, vel, llh);

            TimeStepData stepData;
            stepData.mse = mse;
            stepData.hasError = false;

            // copy stepdata
            for (int j = 0; j < 3; j++)
            {
                stepData.pos[j] = pos[j];
                stepData.vel[j] = vel[j];
                stepData.llh[j] = llh[j];
            }

            // Error or decay condition
            if (errCode != 0)
            {
                GetLastErrMsg(errMsg);
                errMsg[LOGMSGLEN - 1] = 0;

                stepData.hasError = true;
                stepData.errorMsg = std::string(errMsg);
                satData.timeSteps.push_back(stepData);
                satData.propagationSuccess = false;
                break; // Move to the next satellite
            }

            //Compute/Retrieve other propagator output data
            //----------------------------------------------------------------
            Sgp4GetPropOut(pSatKeys[i], XF_SGP4OUT_OSC_KEP, oscKep);
            Sgp4GetPropOut(pSatKeys[i], XF_SGP4OUT_MEAN_KEP, meanKep);
            Sgp4GetPropOut(pSatKeys[i], XF_SGP4OUT_NODAL_AP_PER, nodalApPer);

            // Copy Keplerian elements and nodal data
            for (int j = 0; j < 6; j++)
            {
                stepData.meanKep[j] = meanKep[j];
                stepData.oscKep[j] = oscKep[j];
            }

            for (int j = 0; j < 3; j++)
            {
                stepData.nodalApPer[j] = nodalApPer[j];
            }

            // Calculate mean motion
            stepData.meanMotion = AToN(meanKep[0]);

            // Height is below 100km - Skip the satellite
            if (llh[2] < 100.0)
            {
                stepData.hasError = true;
                if (llh[2] < 0)
                    stepData.errorMsg = "Warning: Decay condition. Distance from the Geoid (Km) = " +
                                       std::to_string(llh[2]);
                else
                    stepData.errorMsg = "Warning: Height is low. HT (Km) = " + std::to_string(llh[2]);

                satData.timeSteps.push_back(stepData);
                satData.propagationSuccess = false;
                break; // Move to the next satellite
            }

            // Add this successful timestep to the satellite data
            satData.timeSteps.push_back(stepData);
            step++;
        }

        // Add this satellite's data to results
        results.satellites.push_back(satData);

        // Remove this satellite if no longer needed
        if (Sgp4RemoveSat(pSatKeys[i]) != 0)
        {
            results.overallSuccess = false;
            results.generalError = "Failed to remove satellite from memory";
            break;
        }
    }

    // Clean up memory
    free(pSatKeys);

    // Clean up after each job
    TleRemoveAllSats();
    Sgp4RemoveAllSats();

    return results;
}

   void PrintHeader(FILE* fp, int fileType) // output file header print
    {
       int startFrEpoch, stopFrEpoch;
       double startTime, stopTime, stepSize;

       startFrEpoch = stopFrEpoch = 0;
       startTime = stopTime = stepSize = 0;

       // Get prediction control data
       Get6P(&startFrEpoch, &stopFrEpoch, &startTime, &stopTime, &stepSize);
       if(startFrEpoch)
          fprintf(fp, "%s%14.4f%s\n", "Start Time = ", startTime, " min from epoch");
       else
          fprintf(fp, "%s%s\n", "Start Time = ", UTCToDtg20Str(startTime));


       if(stopFrEpoch)
          fprintf(fp, "%s%14.4f%s\n", "Stop Time  = ", stopTime,  " min from epoch");
       else
          fprintf(fp, "%s%s\n", "Stop Time  = ", UTCToDtg20Str(stopTime));

       fprintf(fp, "%s%14.4f%s\n\n\n", "Step size  = ", stepSize,  " min");

       if (fileType == FT_OSC_STATE)
       {
          fprintf(fp, "%s\n",
             "     TSINCE (MIN)           X (KM)           Y (KM)           Z (KM)      XDOT (KM/S)       YDOT(KM/S)    ZDOT (KM/SEC)");
       }
       else if (fileType == FT_OSC_ELEM)
       {
          fprintf(fp, "%s\n",
             "     TSINCE (MIN)           A (KM)          ECC (-)        INC (DEG)       NODE (DEG)      OMEGA (DEG)   TRUE ANOM(DEG)");
       }
       else if (fileType == FT_MEAN_ELEM)
       {
          fprintf(fp, "%s\n",
             "     TSINCE (MIN)     N (REVS/DAY)          ECC (-)        INC (DEG)       NODE (DEG)      OMEGA (DEG)         MA (DEG)");
       }
       else if (fileType == FT_LLH_ELEM)
       {
          fprintf(fp, "%s\n",
             "     TSINCE (MIN)         LAT(DEG)        LON (DEG)          HT (KM)           X (KM)           Y (KM)           Z (KM)");
       }
       else if (fileType == FT_NODAL_AP_PER)
       {
          fprintf(fp, "%s\n",
             "     TSINCE (MIN)   NODAL PER(MIN)1/NODAL(REVS/DAY)       N(REVS/DY)    ANOM PER(MIN)      APOGEE (KM)      PERIGEE(KM)");
       }
    }

   // Print position and velocity vectors
void Propagator::PrintPosVel(FILE* fp, double mse, double* pos, double* vel)
{
   fprintf(fp, " %17.7f%17.7f%17.7f%17.7f%17.7f%17.7f%17.7f\n",
      mse, pos[0], pos[1], pos[2], vel[0], vel[1], vel[2]);

}



// Print osculating Keplerian elements
void Propagator::PrintOscEls(FILE* fp, double mse, double* oscKep)
{
   double trueAnomaly = CompTrueAnomaly(oscKep);

   fprintf(fp, " %17.7f%17.7f%17.7f%17.7f%17.7f%17.7f%17.7f\n",
      mse, oscKep[0], oscKep[1], oscKep[2], oscKep[4], oscKep[5], trueAnomaly);

}


// Print mean Keplerian elements
void Propagator::PrintMeanEls(FILE* fp, double mse, double* meanKep)
{
   double meanMotion = AToN(meanKep[0]);

   fprintf(fp, " %17.7f%17.7f%17.7f%17.7f%17.7f%17.7f%17.7f\n",
      mse, meanMotion, meanKep[1], meanKep[2], meanKep[4], meanKep[5], meanKep[3]);

}


// Print geodetic latitude longitude altitude and position vector
void Propagator::PrintLLH(FILE* fp, double mse, double* llh, double* pos)
{
   fprintf(fp, " %17.7f%17.7f%17.7f%17.7f%17.7f%17.7f%17.7f\n",
      mse, llh[0], llh[1], llh[2], pos[0], pos[1], pos[2]);

}


// Print nodal perdiod, apogee, perigee
void Propagator::PrintNodalApPer(FILE* fp, double mse, double n, double* nodalApPer)
{

   fprintf(fp, " %17.7f%17.7f%17.7f%17.7f%17.7f%17.7f%17.7f\n",
      mse,
      nodalApPer[0],
      (1440.0 / nodalApPer[0]),
      n ,
      1440.0 / n,
      nodalApPer[1],
      nodalApPer[2]);
}


// Calculate start/stop times and step size from 6P card (TimeFunc dll)
void Propagator::CalcStartStopTime(double epoch, double* tStart, double* tStop, double* tStep)
{
   int startFrEpoch, stopFrEpoch;
   double startTime, stopTime, stepSize;

   startFrEpoch = stopFrEpoch = 0;
   startTime = stopTime = stepSize = 0;

   // Get prediction control data from 6P card
   Get6P(&startFrEpoch, &stopFrEpoch, &startTime, &stopTime, &stepSize);

   // Compute start/stop times - using days since 1950 UTC
   // user selects start time in minutes since epoch
   if (startFrEpoch == 1)
      *tStart = epoch + (startTime / 1440);
   else // user selects start time in days since 1950 UTC
      *tStart = startTime;

   // user selects stop time in minutes since epoch
   if (stopFrEpoch == 1)
      *tStop = epoch + (stopTime / 1440);
   else // user selects stop time in days since 1950 UTC
      *tStop = stopTime;

   if(*tStart > *tStop)
      *tStep = -fabs(stepSize);
   else
      *tStep = fabs(stepSize);
}

} // SGP_IMPL