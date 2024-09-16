
# Introduction

This application generates keyboard or mouse actions from serial port messages.

---

## Simple steps to learn how the app works:

- Get an [Arduino Micro](https://store.arduino.cc/products/arduino-micro),
- Install [Arduino IDE](https://www.arduino.cc/en/software)
- Double-clicking on `arduino_sketch.ino` starts the `IDE`, and loads the source code.
- The first time it says that the arduino project should be in its own folder and offers to move it, click `Yes`.
- Connect your `Arduino Micro` device, and select it in the drop-down menu of the `IDE`.
- `Sketch` / `Upload` flashes this sketch (7312 bytes flash / 263 bytes ram)
- You can find a `Serial Monitor` below the source code editor. If not, hit `Tools` / `Serial Monitor`.
- Write `+1864` into the message field, hit `Enter`, and watch what happens to your mouse cursor.

Check the #defines in the source code for the supported actions.

---

**WARNING!**

**Do not leave your computer unattended while such a device is connected to it.**
**The computer does not distinguish between mouse and keyboard operations performed by you or the device.**
**Using such a device may compromise IT security.**

