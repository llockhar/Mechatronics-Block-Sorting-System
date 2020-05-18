# Mechatronics-Block-Sorting-System
Programming a conveyor system to sort blocks using AVR microcontroller 

## System Components
The sorting system has two motors and three sensors all controlled with C code on a microcontroller:
* 8-bit AVR Microcontroller: ATmega32U6
* Conveyor motor: L298 Dual Full-Bridge Motor Driver
* Conveyor exit gate: OPB819Z Slotted Optical Switch
* Sorting bin motor: Soyo 6V 0.8A 36oz-in Unipolar Stepper Motor
* Metal detector: E2R Flat Proximity Sensor
* Optical sensor: SFH 310 FA Silicon NPN Phototransistor

## System Function
Blocks of different materials (white plastic, black plastic, steel, aluminum) were placed on a conveyor belt. Along the conveyor, they passed a metal detector which could differentiate plastic from metal. Next, they passed an optical sensor that was used to classify white/black plastic and steel/aluminum. An optical switch at the end of the conveyor signaled when a block had arrived. A sorting bin below the end of the conveyor had a section for each block material. 

The conveyor had to stop until the sorting bin was at the right section. Duration and direction that the sorting bin stepper motor was on or off was carefully optimized for speed. Sensor values were retrieved using interrupts. Multiple metal and optical detector values were taken by the time a block reached the end of the conveyor, and linked queues kept track of which sensor values belonged to which block. The conveyor speed was optimized to move the blocks as quickly as possible while still reading accurate sensor measurements.
