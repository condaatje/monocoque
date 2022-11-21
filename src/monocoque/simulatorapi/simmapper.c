#include <stdio.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

#include "simmapper.h"
#include "simdata.h"
#include "test.h"
#include "ac.h"
#include "rf2.h"
#include "../helper/confighelper.h"
#include "../slog/slog.h"

#include "simapi/acdata.h"
#include "simapi/rf2data.h"


int simdatamap(SimData* simdata, SimMap* simmap, Simulator simulator)
{
    switch ( simulator )
    {
        case SIMULATOR_MONOCOQUE_TEST :
            memcpy(simdata, simmap->addr, sizeof(SimData));
            break;
        case SIMULATOR_ASSETTO_CORSA :
            memcpy(&simmap->d.ac.ac_physics, simmap->d.ac.physics_map_addr, sizeof(simmap->d.ac.ac_physics));
            if (simmap->d.ac.has_static == true )
            {
                memcpy(&simmap->d.ac.ac_static, simmap->d.ac.static_map_addr, sizeof(simmap->d.ac.ac_static));
                simdata->maxrpm = simmap->d.ac.ac_static.maxRpm;
            }
            simdata->rpms = simmap->d.ac.ac_physics.rpms;
            simdata->gear = simmap->d.ac.ac_physics.gear;
            simdata->velocity = simmap->d.ac.ac_physics.speedKmh;
            simdata->altitude = 1;
            break;
        case SIMULATOR_RFACTOR2 :
            memcpy(&simmap->d.rf2.rf2_telemetry, simmap->d.rf2.telemetry_map_addr, sizeof(simmap->d.rf2.rf2_telemetry));
           
            simdata->velocity = abs(ceil(simmap->d.rf2.rf2_telemetry.mVehicles[0].mLocalVel.z * 3.6));
            simdata->rpms = ceil(simmap->d.rf2.rf2_telemetry.mVehicles[0].mEngineRPM);
            simdata->gear = simmap->d.rf2.rf2_telemetry.mVehicles[0].mGear;
            //simdata->gear = simmap->d.rf2.rf2_telemetry.mVehicles[0].mLapNumber;
            simdata->maxrpm - ceil(simmap->d.rf2.rf2_telemetry.mVehicles[0].mEngineMaxRPM);
            break;
    }
}


int siminit(SimData* simdata, SimMap* simmap, Simulator simulator)
{
    slogi("searching for simulator data...");
    int error = MONOCOQUE_ERROR_NONE;

    void* a;
    switch ( simulator )
    {
        case SIMULATOR_MONOCOQUE_TEST :
            simmap->fd = shm_open(TEST_MEM_FILE_LOCATION, O_RDONLY, S_IRUSR | S_IWUSR);
            if (simmap->fd == -1)
            {
                return 10;
            }

            simmap->addr = mmap(NULL, sizeof(SimData), PROT_READ, MAP_SHARED, simmap->fd, 0);
            if (simmap->addr == MAP_FAILED)
            {
                return 30;
            }
            slogi("found data for monocoque test...");
            break;
        case SIMULATOR_ASSETTO_CORSA :

            simmap->d.ac.has_physics=false;
            simmap->d.ac.has_static=false;
            simmap->fd = shm_open(AC_PHYSICS_FILE, O_RDONLY, S_IRUSR | S_IWUSR);
            if (simmap->fd == -1)
            {
                slogd("could not open Assetto Corsa physics engine");
                return MONOCOQUE_ERROR_NODATA;
            }
            simmap->d.ac.physics_map_addr = mmap(NULL, sizeof(simmap->d.ac.ac_physics), PROT_READ, MAP_SHARED, simmap->fd, 0);
            if (simmap->d.ac.physics_map_addr == MAP_FAILED)
            {
                slogd("could not retrieve Assetto Corsa physics data");
                return 30;
            }
            simmap->d.ac.has_physics=true;

            simmap->fd = shm_open(AC_STATIC_FILE, O_RDONLY, S_IRUSR | S_IWUSR);
            if (simmap->fd == -1)
            {
                slogd("could not open Assetto Corsa static data");
                return 10;
            }
            simmap->d.ac.static_map_addr = mmap(NULL, sizeof(simmap->d.ac.ac_static), PROT_READ, MAP_SHARED, simmap->fd, 0);
            if (simmap->d.ac.static_map_addr == MAP_FAILED)
            {
                slogd("could not retrieve Assetto Corsa static data");
                return 30;
            }
            simmap->d.ac.has_static=true;

            slogi("found data for Assetto Corsa...");
            break;
        
        case SIMULATOR_RFACTOR2 :

            simmap->d.rf2.has_telemetry=false;
            simmap->d.rf2.has_scoring=false;
            simmap->fd = shm_open(RF2_TELEMETRY_FILE, O_RDONLY, S_IRUSR | S_IWUSR);
            if (simmap->fd == -1)
            {
                slogd("could not open RFactor2 Telemetry engine");
                return MONOCOQUE_ERROR_NODATA;
            }
            simmap->d.rf2.telemetry_map_addr = mmap(NULL, sizeof(simmap->d.rf2.rf2_telemetry), PROT_READ, MAP_SHARED, simmap->fd, 0);
            if (simmap->d.rf2.telemetry_map_addr == MAP_FAILED)
            {
                slogd("could not retrieve RFactor2 telemetry data");
                return 30;
            }
            simmap->d.rf2.has_telemetry=true;


            slogi("found data for RFactor2...");
            break;
    }

    return error;
}
