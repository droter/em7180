/* 
   Altitude.hpp: Altitude estimator using baro + accelerometer fusion

   Adapted from

    https://github.com/multiwii/baseflight/blob/master/src/imu.c
    https://github.com/multiwii/baseflight/blob/master/src/sensors.c

   This file is part of EM7180.

   EM7180 is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   EM7180 is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   You should have received a copy of the GNU General Public License
   along with EM7180.  If not, see <http://www.gnu.org/licenses/>.
 */

#define ACCEL_1G             2048
#define ACCEL_Z_LPF_CUTOFF   5.0f
#define ACCEL_Z_DEADBAND     40
#define ACCEL_LPF_FACTOR     4

#define BARO_TAB_SIZE        48
#define BARO_CALIBRATION_SEC 8
#define BARO_NOISE_LPF       0.5f

class Altitude {

    public:

        Altitude(void);

        void updateBaro(float pressure);

        void updateImu(int16_t accelRaw[3], float eulerAngles[3]);

    private:

        // IMU

        float     accelLpf[3];
        int32_t   accelZSum;
        float     accelVelScale;
        int32_t   accelZOffset;
        float     accelZSmooth;
        int16_t   accelSmooth[3];
        uint32_t  accelTimeSum;
        int32_t   accelSumCount;
        float     accelFc;

        static int32_t deadbandFilter(int32_t value, int32_t deadband);
        static void    rotateV(float v[3], float *delta);

        // Baro

        uint32_t baroPressureSum;
        int32_t  baroHistTab[BARO_TAB_SIZE];
        int      baroHistIdx;
        int32_t  baroGroundPressure;
        int32_t  baroGroundAltitude;
        int32_t  BaroAlt;
        uint32_t baroCalibrationStart;

        static float paToCm(uint32_t pa);
};

/********************************************* CPP ********************************************************/

// Pressure in Pascals to altitude in centimeters
float Altitude::paToCm(uint32_t pa)
{
    return (1.0f - powf(pa / 101325.0f, 0.190295f)) * 4433000.0f;
}

Altitude::Altitude(void)
{
    accelVelScale = 9.80665f / ACCEL_1G / 10000.0f;

    for (int k=0; k<3; ++k) {
        accelSmooth[k] = 0;
        accelLpf[k] = 0;
    }
    accelZOffset = 0;
    accelTimeSum = 0;
    accelSumCount = 0;
    accelZSmooth = 0;
    accelZSum = 0;

    // Calculate RC time constant used in the accelZ lpf    
    accelFc = (float)(0.5f / (M_PI * ACCEL_Z_LPF_CUTOFF)); 

    baroPressureSum = 0;
    baroHistIdx = 0;
    baroGroundPressure = 0;
    baroGroundAltitude = 0;
    BaroAlt = 0;
    baroCalibrationStart = 0;

    for (int k=0; k<BARO_TAB_SIZE; ++k) {
        baroHistTab[k] = 0;
    }
}

void Altitude::updateBaro(float pressure)
{  
    // Start baro calibration if not yet started
    if (!baroCalibrationStart) 
        baroCalibrationStart = millis();

    // Smoothe baro pressure using history
    uint8_t indexplus1 = (baroHistIdx + 1) % BARO_TAB_SIZE;
    baroHistTab[baroHistIdx] = (int32_t)pressure;
    baroPressureSum += baroHistTab[baroHistIdx];
    baroPressureSum -= baroHistTab[indexplus1];
    baroHistIdx = indexplus1;

    // Compute baro ground altitude during calibration
    if (millis() - baroCalibrationStart < 1000*BARO_CALIBRATION_SEC) {
        baroGroundPressure -= baroGroundPressure / 8;
        baroGroundPressure += baroPressureSum / (BARO_TAB_SIZE - 1);
        baroGroundAltitude = paToCm(baroGroundPressure/8);
    }

    int32_t BaroAlt_tmp = paToCm((float)baroPressureSum/(BARO_TAB_SIZE-1)); 
    BaroAlt_tmp -= baroGroundAltitude;
    BaroAlt = lrintf((float)BaroAlt * BARO_NOISE_LPF + (float)BaroAlt_tmp * (1.0f - BARO_NOISE_LPF)); // additional LPF to reduce baro noise
}

void Altitude::updateImu(int16_t accelRaw[3], float eulerAngles[3])
{
    // Track delta time
    static uint32_t previousTimeUsec;
    uint32_t dT_usec = micros() - previousTimeUsec;
    previousTimeUsec = micros();

    for (uint8_t k=0; k<3; k++) {
        if (ACCEL_LPF_FACTOR > 0) {
            accelLpf[k] = accelLpf[k] * (1.0f - (1.0f / ACCEL_LPF_FACTOR)) + accelRaw[k] * (1.0f / ACCEL_LPF_FACTOR);
            accelSmooth[k] = accelLpf[k];
        } else {
            accelSmooth[k] = accelRaw[k];
        }
    }

    // Rotate accel values into the earth frame

    float rpy[3];
    rpy[0] = -(float)eulerAngles[0];
    rpy[1] = -(float)eulerAngles[1];
    rpy[2] = -(float)eulerAngles[2];


    Serial.print("Accel: ");
    Serial.println(accelSmooth[2]);

    float       accelNed[3];
    accelNed[0] = accelSmooth[0];
    accelNed[1] = accelSmooth[1];
    accelNed[2] = accelSmooth[2];
    rotateV(accelNed, rpy);

    // Compute vertical acceleration offset at rest
    if (true /*!armed*/) {
        accelZOffset -= accelZOffset / 64;
        accelZOffset += (int32_t)accelNed[2];
    }

    // Compute smoothed vertical acceleration
    accelNed[2] -= accelZOffset / 64;  // compensate for gravitation on z-axis
    float dT_sec = dT_usec * 1e-6f;
    accelZSmooth = accelZSmooth + (dT_sec / (accelFc + dT_sec)) * (accelNed[2] - accelZSmooth); // low pass filter

    // Apply Deadband to reduce integration drift and vibration influence and
    // sum up Values for later integration to get velocity and distance
    accelZSum += deadbandFilter((int32_t)lrintf(accelZSmooth),  ACCEL_Z_DEADBAND);

    // Accumulate time and count for integrating accelerometer values
    accelTimeSum += dT_usec;
    accelZSum += deadbandFilter((int32_t)lrintf(accelZSmooth),  ACCEL_Z_DEADBAND);
}

void Altitude::rotateV(float v[3], float *delta)
{
    float * v_tmp = v;

    // This does a  "proper" matrix rotation using gyro deltas without small-angle approximation
    float mat[3][3];
    float cosx, sinx, cosy, siny, cosz, sinz;
    float coszcosx, sinzcosx, coszsinx, sinzsinx;

    cosx = cosf(delta[0]);
    sinx = sinf(delta[0]);
    cosy = cosf(delta[1]);
    siny = sinf(delta[1]);
    cosz = cosf(delta[2]);
    sinz = sinf(delta[2]);

    coszcosx = cosz * cosx;
    sinzcosx = sinz * cosx;
    coszsinx = sinx * cosz;
    sinzsinx = sinx * sinz;

    mat[0][0] = cosz * cosy;
    mat[0][1] = -cosy * sinz;
    mat[0][2] = siny;
    mat[1][0] = sinzcosx + (coszsinx * siny);
    mat[1][1] = coszcosx - (sinzsinx * siny);
    mat[1][2] = -sinx * cosy;
    mat[2][0] = (sinzsinx) - (coszcosx * siny);
    mat[2][1] = (coszsinx) + (sinzcosx * siny);
    mat[2][2] = cosy * cosx;

    v[0] = v_tmp[0] * mat[0][0] + v_tmp[1] * mat[1][0] + v_tmp[2] * mat[2][0];
    v[1] = v_tmp[0] * mat[0][1] + v_tmp[1] * mat[1][1] + v_tmp[2] * mat[2][1];
    v[2] = v_tmp[0] * mat[0][2] + v_tmp[1] * mat[1][2] + v_tmp[2] * mat[2][2];
}


int32_t Altitude::deadbandFilter(int32_t value, int32_t deadband)
{
    if (abs(value) < deadband) {
        value = 0;
    } else if (value > 0) {
        value -= deadband;
    } else if (value < 0) {
        value += deadband;
    }
    return value;
}
