# esp32-rideon
ESP32-based transmitter and receiver (+DC motor drivers and servo driver) that can be used for ride ons.

# Transmitter

The transmitter is a small esp32 board connected to my Jumper T12 (or any radio running openTX). 
Pin 1 (top most): SUS (at 7.4V, so use a zener diode or other protection for your esp32)
Pin 3: 7.4V (battery voltage of your radio)
Pin 4: GND

schematics will follow.

# Receiver

Currently, the receiver is a R32 D1 board that breaks out the pins of its ESP32 to the Arduino formfactor. On top, I use an HW-130 shield. This is a clone of the very old Adafruit Motorshield V1. It has two L293 drivers connected to a 74HC595. It also supports two servos.

schematics will follow - or maybe I switch to a different hardware.
