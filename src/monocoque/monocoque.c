#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <string.h>

#include <libconfig.h>
#include "gameloop/gameloop.h"
#include "gameloop/tachconfig.h"
#include "devices/simdevice.h"
#include "helper/parameters.h"
#include "helper/dirhelper.h"
#include "helper/confighelper.h"
#include "simulatorapi/simapi/simapi/simdata.h"
#include "slog/slog.h"

int create_dir(char* dir)
{
    struct stat st = {0};
    if (stat(dir, &st) == -1)
    {
        mkdir(dir, 0700);
    }
}

char* create_user_dir(char* dirtype)
{
    char* home_dir_str = gethome();
    char* config_dir_str = ( char* ) malloc(1 + strlen(home_dir_str) + strlen(dirtype) + strlen("monocoque/"));
    strcpy(config_dir_str, home_dir_str);
    strcat(config_dir_str, dirtype);
    strcat(config_dir_str, "monocoque");

    create_dir(config_dir_str);
    free(config_dir_str);
}

void display_banner()
{
    printf("______  ______________   ___________________________________  ___________\n");
    printf("___   |/  /_  __ \\__  | / /_  __ \\_  ____/_  __ \\_  __ \\_  / / /__  ____/\n");
    printf("__  /|_/ /_  / / /_   |/ /_  / / /  /    _  / / /  / / /  / / /__  __/   \n");
    printf("_  /  / / / /_/ /_  /|  / / /_/ // /___  / /_/ // /_/ // /_/ / _  /___   \n");
    printf("/_/  /_/  \\____/ /_/ |_/  \\____/ \\____/  \\____/ \\___\\_\\\\____/  /_____/   \n");
}

int main(int argc, char** argv)
{
    display_banner();

    Parameters* p = malloc(sizeof(Parameters));
    MonocoqueSettings* ms = malloc(sizeof(MonocoqueSettings));;

    ConfigError ppe = getParameters(argc, argv, p);
    if (ppe == E_SUCCESS_AND_EXIT)
    {
        goto cleanup_final;
    }
    ms->program_action = p->program_action;

    char* home_dir_str = gethome();
    create_user_dir("/.config/");
    create_user_dir("/.cache/");
    char* config_file_str = ( char* ) malloc(1 + strlen(home_dir_str) + strlen("/.config/") + strlen("monocoque/monocoque.config"));
    char* cache_dir_str = ( char* ) malloc(1 + strlen(home_dir_str) + strlen("/.cache/monocoque/"));
    strcpy(config_file_str, home_dir_str);
    strcat(config_file_str, "/.config/");
    strcpy(cache_dir_str, home_dir_str);
    strcat(cache_dir_str, "/.cache/monocoque/");
    strcat(config_file_str, "monocoque/monocoque.config");

    slog_config_t slgCfg;
    slog_config_get(&slgCfg);
    slgCfg.eColorFormat = SLOG_COLORING_TAG;
    slgCfg.eDateControl = SLOG_TIME_ONLY;
    strcpy(slgCfg.sFileName, "monocoque.log");
    strcpy(slgCfg.sFilePath, cache_dir_str);
    slgCfg.nTraceTid = 0;
    slgCfg.nToScreen = 1;
    slgCfg.nUseHeap = 0;
    slgCfg.nToFile = 1;
    slgCfg.nFlush = 0;
    slgCfg.nFlags = SLOG_FLAGS_ALL;
    slog_config_set(&slgCfg);
    if (p->verbosity_count < 2)
    {
        slog_disable(SLOG_TRACE);
    }
    if (p->verbosity_count < 1)
    {
        slog_disable(SLOG_DEBUG);
    }

    slogi("Loading configuration file: %s", config_file_str);
    config_t cfg;
    config_init(&cfg);
    config_setting_t* config_devices = NULL;

    if (!config_read_file(&cfg, config_file_str))
    {
        fprintf(stderr, "%s:%d - %s\n", config_error_file(&cfg), config_error_line(&cfg), config_error_text(&cfg));
    }
    else
    {
        slogi("Openend monocoque configuration file");
        config_devices = config_lookup(&cfg, "devices");
    }
    free(config_file_str);
    free(cache_dir_str);

    if (p->program_action == A_CONFIG_TACH)
    {
        int error = 0;
        SimDevice* tachdev = malloc(sizeof(SimDevice));
        SimData* sdata = malloc(sizeof(SimData));
        DeviceSettings* ds = malloc(sizeof(DeviceSettings));

        error = devsetup("USB", "Tachometer", "None", ms, ds, NULL);

        if (error != MONOCOQUE_ERROR_NONE)
        {
            sloge("Could not proceed with tachometer configuration due to error: %i", error);
        }
        else
        {
            error = devinit(tachdev, 1, ds);
        }

        if (error != MONOCOQUE_ERROR_NONE)
        {
            sloge("Could not proceed with tachometer configuration due to error: %i", error);
        }
        else
        {
            slogi("configuring tachometer with max revs: %i, granularity: %i, saving to %s", p->max_revs, p->granularity, p->save_file);
            config_tachometer(p->max_revs, p->granularity, p->save_file, tachdev, sdata);
        }
        devfree(tachdev, 1);
        //free(tachdev);
        free(sdata);
        free(ds);
    }
    else
    {

        int error = 0;

        int configureddevices = config_setting_length(config_devices);
        int numdevices = 0;
        DeviceSettings ds[configureddevices];
        slogi("found %i devices in configuration", configureddevices);
        int i = 0;
        while (i<configureddevices)
        {
            error = MONOCOQUE_ERROR_NONE;
            DeviceSettings settings;

            config_setting_t* config_device = config_setting_get_elem(config_devices, i);
            const char* device_type;
            const char* device_subtype;
            const char* device_config_file;
            config_setting_lookup_string(config_device, "device", &device_type);
            config_setting_lookup_string(config_device, "type", &device_subtype);
            config_setting_lookup_string(config_device, "config", &device_config_file);

            if (error == MONOCOQUE_ERROR_NONE)
            {
                error = devsetup(device_type, device_subtype, device_config_file, ms, &settings, config_device);
            }
            if (error == MONOCOQUE_ERROR_NONE)
            {
                numdevices++;
            }
            ds[i] = settings;

            i++;

        }

        i = 0;
        int j = 0;
        error = MONOCOQUE_ERROR_NONE;
        SimDevice* devices = malloc(numdevices * sizeof(SimDevice));
        int initdevices = devinit(devices, configureddevices, ds);

        if (p->program_action == A_PLAY)
        {
            //slogi("running monocoque in gameloop mode..");
            //error = strtogame(p->sim_string, ms);
            //if (error != MONOCOQUE_ERROR_NONE)
            //{
            //    goto cleanup_final;
            //}


            error = looper(devices, initdevices, p);
            if (error == MONOCOQUE_ERROR_NONE)
            {
                slogi("Game loop exited succesfully with error code: %i", error);
            }
            else
            {
                sloge("Game loop exited with error code: %i", error);
            }
        }
        else
        {
            slogi("running monocoque in test mode...");
            error = tester(devices, initdevices);
            if (error == MONOCOQUE_ERROR_NONE)
            {
                slogi("Test exited succesfully with error code: %i", error);
            }
            else
            {
                sloge("Test exited with error code: %i", error);
            }
        }


        devfree(devices, initdevices);

        i = 0;
        // improve the api around the config helper
        // i don't like that i stack allocated but hid a malloc inside
        for( i = 0; i < configureddevices; i++)
        {
            settingsfree(ds[i]);
        }
    }



configcleanup:
    config_destroy(&cfg);

cleanup_final:
    free(ms);
    free(p);
    exit(0);
}


