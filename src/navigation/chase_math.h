#pragma once

#include "../gps/local_gps.h"
#include "../models/navigation_info.h"
#include "../models/sonde_telemetry.h"

NavigationInfo calculateNavigation(
    LocalGps& localGps,
    const SondeTelemetry& sonde,
    bool sondePositionUsable
);
