/* 
   Interrup.ino: Example sketch for running EM7180 SENtral sensor hub in master mode with interrupts

   Adapted from

     https://github.com/kriswiner/EM7180_SENtral_sensor_hub/tree/master/WarmStartandAccelCal

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

#include "EM7180.h"

#ifdef __MK20DX256__
#include <i2c_t3.h>
#define NOSTOP I2C_NOSTOP
#else
#include <Wire.h>
#define NOSTOP false
#endif

// This works for LadybugFC; you should change it for your controller.
static const uint8_t INTERRUPT_PIN = 12;

extern volatile bool newData;

EM7180 em7180;

void setup()
{
#ifdef __MK20DX256__
    Wire.begin(I2C_MASTER, 0x00, I2C_PINS_18_19, I2C_PULLUP_EXT, I2C_RATE_400);
#else
    Wire.begin();
#endif

    delay(100);

    Serial.begin(38400);

    // Start the EM7180 in master mode with interrupt
    if (!em7180.begin(8, 2000, 1000, INTERRUPT_PIN)) {

        while (true) {
            Serial.println(em7180.getErrorString());
        }
    }
}

void loop()
{  
    if (em7180.gotInterrupt()) {
        Serial.println(millis());
    }

    /*
    em7180.checkEventStatus();

    if (em7180.gotError()) {
        Serial.print("ERROR: ");
        Serial.println(em7180.getErrorString());
        return;
    }*/

    delay(10);
}
