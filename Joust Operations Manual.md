# Joust Operations Manual

![][image1]  
![][image2]

Prepared by: Nick Greeno \- [nickgreeno@gmail.com](mailto:nickgreeno@gmail.com) \- (902) \- 830 \- 9883 or (782) 777-8822  
Prepared on: 2025-03-14  
Last edited: 2025-04-02

# System Overview

## Mechanical Systems

### Main Chassis

![][image3]

### Knight Sled

![][image4]

## Arduino Pinout

### Generic

**![][image5]**

### Actual

![][image6]

## 

# Program Operation

## Safe Operation Mode

When Joust is in Safe Operation Mode it will perform a series of checks to see if it is safe to operate. If these checks are passed the program will bypass the keyed lock and manual modes, perform an automatic reset, and proceed to normal operation. 

To engage safe operation mode, connect a jumper from any GND pin to the A12 pin (Analog 12).

Manual mode is still accessible while the Safe Operation jumper is installed. To enter manual mode, hold down the \[START\] button during the bootup process. This will initiate the manual control mode. In the unlikely event that Joust restarts and a patron is holding the start button the program will enter manual operation however, they will only have access to the \[START\] button so they will not be able to control the machine. Should they press the \[START\] button to exit manual mode the program will again check to see if it is in a recoverable state and try to auto-reset. If it is not safe to reset the program will hang, waiting for an operator to access the control panel and reset the machine to a safe position or decommission for repairs.

With the Safe Operation jumper removed, the program will progress through the phases listed below: Boot, Manual, Reset, and Normal Operation.

## On Boot

Joust starts in locked state so long as the Safe Operation jumper is not installed. No user input will be recognized in this state, no motors can engage. In the event of a critical failure the program will timeout and reset after 2 seconds, returning to this state.

When locked, the start button will turn on for 3 seconds, then turn off for 3 seconds.

Joust is unlocked by using the key. Insert key and turn to unlock. This will enter manual mode. When in manual mode, the start button will turn on for one second, then turn off for one second. Once in manual mode the key can return to the locked position and be removed.

## Manual Operation

The user has full control over all motors and can simulate limit switch triggers. Limit switches have been implemented such that the motors will not overdrive.

**WARNING: While in manual mode nights can rotate while in the middle of the track, potentially causing a collision with the fence.**

To exit manual mode, press the start button on the PCB or the main start button on the display.

### Control Panel

![][image7]

1. Chain Drive CCW \- Engages the chain drive motor in the counter clockwise direction. Knight 1 (closest to user) moves the to left, Knight 2 (furthest from user) moves to the right. Pressing button \[11\] will trigger the chain drive CCW limit switch to stop the chain drive motor.  
2. Chain Drive CW \- Engages the chain drive motor in the clockwise direction. Knight 1 (closest to user) moves the to right, Knight 2 (furthest from user) moves to the left. Pressing button \[9\] will trigger the chain drive CW limit switch to stop the chain drive motor.  
3. Both Knights Rotate CW \- Both knights will rotate in CW direction. Pressing button \[15\] will stop Knight 1\. Pressing button \[5\] will stop Knight 2\.  
4. Both Knights Rotate CCW \- Both knights will rotate in CCW direction. Pressing button \[14\] will stop Knight 1\. Pressing button \[6\] will stop Knight 2\.  
5. Knight 2 \- CW Rotation Limit Switch \- Triggers the end of Knight 2 CW rotation.  
6. Knight 2 \- CCW Rotation Limit Switch \- Triggers the end of Knight 2 CCW rotation.  
7. Knight 2 \- Lance Down Limit Switch \- Triggers the end of Knight 2 lance down movement.  
8. Knight 2 \- Lance Up Limit Switch \- Triggers the end of Knight 2 lance up movement.  
9. Chain Drive CW Limit Switch \- Triggers the end of the chain drive CW rotation.  
10. Start Button \- Emulates the Start button when installed in display case. Exits Manual mode and initiates reset mode.  
11. Chain Drive CCW Limit Switch \- Triggers the end of the chain drive CCW rotation.  
12. Knight 1 \- Lance Up Limit Switch \- Triggers the end of Knight 2 lance up movement.  
13. Knight 1 \- Lance Down Limit Switch \- Triggers the end of Knight 1 lance down movement.  
14. Knight 1 \- CCW Rotation Limit Switch \- Triggers the end of Knight 1 CCW rotation.  
15. Knight 1 \- CW Rotation Limit Switch \- Triggers the end of Knight 1 CW rotation.  
16. Both Lance Up \- Both knights will raise their lances. Pressing button \[12\] will stop Knight 1\. Pressing button \[8\] will stop Knight 2\.  
17. Both Lances Down \- Both knights will lower their lances. Pressing button \[13\] will stop Knight 1\. Pressing button \[7\] will stop Knight 2\.

## Reset Mode

Once manual mode is exited the program will go into reset mode. The machine will run a series of checks to determine where the knights are located, their rotation, and their lance position. In most cases the reset will be automatic, however, there is one condition that would require user input, this is unlikely to ever happen during normal operations. This condition could be achieved using manual mode.

Once the reset has been completed the program will enter the Joust routine.

### 

### Fully Automatic Reset Conditions

If either the chain drive CW or CCW limit switch is closed, the lances with raise and knights will rotate to face the appropriate direction.

If either of the knights are fully rotated CW or CCW the other knight will be rotated to match orientation and the chain drive will engage in the appropriate direction to return the knights “home”.

### Semi-Automatic Reset Condition

If both chain drive limit switch are open and all 4 rotation limit switches are open the program will wait for the user to select knights rotate CW or CCW (same buttons as in manual mode) then proceed to return the knights to the appropriate “home” location based on their orientation.

The start button will turn on for one second then turn off for one second while waiting for input.

## Joust \- Normal Operation

When ready to joust, the program will turn on the start button light. When pressed, the start button will turn off until the current joust is completed.

### Stages of joust

Salute: The knights will bow their lances.

Charge: The knights will lower their lances then charge towards each other.

Impact: Where the knights would impact each other.

Pass: The phase after impact.

Turn: The knights will raise their lances then turn.

Reset: A winner is declared (at random). This knight will bow his lance. The program is then ready to run again.

# Safety Features

## Limit Switches

Any time a call to engage a motor is made the program checks to ensure that the limit switch is open. If it is closed, that motor will not engage. This is done on a per-motor basis, such that if both knights rotate CCW is called but one is already rotated CCW, the other will still rotate appropriately.

## Watchdog Timer

A watchdog timer of 2 seconds has been implemented. If the program gets stuck for any reason, the Watchdog timer will trigger and reset the program and will not be able to re-engage until unlocked using the key.

## Physical Key

A key slot has been implemented to unlock the machine when first powered up or when the watchdog timer has been triggered.

The key acts as a button. Once the key is turned to the unlock position the program will enter manual mode. Once manual mode is engaged the key should be return to the locked position and removed.

If the key is removed while in the locked position this will not affect the operation of the program. If the watchdog timer is triggered the key will have to be used to lock and then unlock the machine as it is looking for a change in state of the “button”.

## Lance Redundancy Check

During testing it was discovered that the lance up/down limit switches do not always register when engaged. This would cause the program to stall. A timer has been implemented to turn off the lance motors after a specified time and moth to the next phase of the program. During testing, this happened approximately 4/150 runs with no interference to the overall operation of the program. This timer does not trigger the watchdog reset.

## UPS Backup Power

A UPS backup is recommended but not critical. A UPS would ensure that during a power outage or brown out the machine continues operation as normal. This is not necessary as the machine can automatically reset from any power loss condition.

## 

## 

# Arduino Code
See: 2025_04_02_Joust_Deployment_NRG.ino