#include "chase_math.h"

#include <math.h>

namespace {
constexpr double PI_D = 3.14159265358979323846;
constexpr double EARTH_RADIUS_M = 6371000.0;

double degToRad(double degrees) {
    return degrees * PI_D / 180.0;
}

double radToDeg(double radians) {
    return radians * 180.0 / PI_D;
}

double normaliseBearing(double degrees) {
    while (degrees < 0.0) {
        degrees += 360.0;
    }

    while (degrees >= 360.0) {
        degrees -= 360.0;
    }

    return degrees;
}

double haversineDistanceMetres(
    double lat1,
    double lon1,
    double lat2,
    double lon2
) {
    const double phi1 = degToRad(lat1);
    const double phi2 = degToRad(lat2);
    const double dPhi = degToRad(lat2 - lat1);
    const double dLambda = degToRad(lon2 - lon1);

    const double sinDPhi = sin(dPhi / 2.0);
    const double sinDLambda = sin(dLambda / 2.0);

    const double a =
        sinDPhi * sinDPhi +
        cos(phi1) * cos(phi2) * sinDLambda * sinDLambda;

    const double c = 2.0 * atan2(sqrt(a), sqrt(1.0 - a));

    return EARTH_RADIUS_M * c;
}

double initialBearingDegrees(
    double lat1,
    double lon1,
    double lat2,
    double lon2
) {
    const double phi1 = degToRad(lat1);
    const double phi2 = degToRad(lat2);
    const double dLambda = degToRad(lon2 - lon1);

    const double y = sin(dLambda) * cos(phi2);
    const double x =
        cos(phi1) * sin(phi2) -
        sin(phi1) * cos(phi2) * cos(dLambda);

    return normaliseBearing(radToDeg(atan2(y, x)));
}
}

NavigationInfo calculateNavigation(
    LocalGps& localGps,
    const SondeTelemetry& sonde,
    bool sondePositionUsable
) {
    NavigationInfo info;

    info.localFixValid = localGps.fixValid();
    info.sondeFixValid = sondePositionUsable;

    info.localLatitude = localGps.latitude();
    info.localLongitude = localGps.longitude();
    info.localAltitudeMetres = localGps.altitudeMetres();
    info.localSatellites = localGps.satellites();
    info.localHdop = localGps.hdop();
    info.localFixAgeMs = localGps.fixAgeMs();

    if (!info.localFixValid ||
        !info.sondeFixValid ||
        !isfinite(info.localLatitude) ||
        !isfinite(info.localLongitude) ||
        !isfinite(sonde.latitude) ||
        !isfinite(sonde.longitude)) {
        return info;
    }

    info.distanceMetres = haversineDistanceMetres(
        info.localLatitude,
        info.localLongitude,
        sonde.latitude,
        sonde.longitude
    );

    info.bearingDegrees = initialBearingDegrees(
        info.localLatitude,
        info.localLongitude,
        sonde.latitude,
        sonde.longitude
    );

    const double localAltitude =
        isfinite(info.localAltitudeMetres)
            ? info.localAltitudeMetres
            : 0.0;

    const double sondeAltitude =
        isfinite(sonde.altitudeMetres)
            ? sonde.altitudeMetres
            : localAltitude;

    info.relativeAltitudeMetres = sondeAltitude - localAltitude;

    info.straightLineMetres = sqrt(
        (info.distanceMetres * info.distanceMetres) +
        (info.relativeAltitudeMetres * info.relativeAltitudeMetres)
    );

    info.elevationDegrees = radToDeg(
        atan2(info.relativeAltitudeMetres, info.distanceMetres)
    );

    info.navValid =
        isfinite(info.distanceMetres) &&
        isfinite(info.bearingDegrees) &&
        isfinite(info.elevationDegrees);

    return info;
}
