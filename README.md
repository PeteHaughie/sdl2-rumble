# SDL2 Joystick Rumble Utility

This SDL2 utility application provides an API to control gamepad rumble (force feedback) through a TCP socket. It supports multiple joysticks and allows specifying which joystick to target for rumble commands.

## Features

- Enumerates all connected joysticks and opens them.
- Accepts TCP connections to receive rumble commands.
- Supports specifying the joystick index, low-frequency rumble, high-frequency rumble, and duration.
- Handles graceful shutdown with signal handling.

## Why does this exist?

I was working with [openFrameworks](https://www.openframeworks.cc/) and [GLFW](https://github.com/glfw/glfw/) on a project that would have benefitted from force feedback on gamepads which GLFW still doesn't offer despite it being on the roadmap for [more than a decade](https://github.com/glfw/glfw/issues/57). I did find a patch for the functionality in [rumble.zig](https://github.com/machlibs/rumble/blob/main/src/rumble.zig) but had no idea how to implement this in any sort of useful manner so I decided to examine my options. Rather than try and put rumble back into GLFW perhaps I could bridge the gap by using a different framework and accessing it via a simple API.

## API

### Command Format

The command format is:

`<joystick_index> <low_freq> <high_freq> <duration>`

- `joystick_index`: The index of the joystick to target (0-based).
- `low_freq`: The low-frequency rumble value (Uint16).
- `high_freq`: The high-frequency rumble value (Uint16).
- `duration`: The duration of the rumble in milliseconds (Uint32).

Example command:

```
0 32000 16000 5000
```

This command targets joystick 0 with a low-frequency rumble of 32000, high-frequency rumble of 16000, for a duration of 5000 milliseconds.

## Usage

### Running the Utility

1. **Compile the SDL2 utility application:**

   ```sh
   g++ -std=c++11 -o sdl2_joystick src/main.cpp -I/usr/local/include/SDL2 -D_THREAD_SAFE -L/usr/local/lib -lSDL2
   ```
   or just run the build.sh file as it contains the same instruction.

2. **Run the compiled application:**

   ```sh
   ./sdl2_joystick
   ```

### Sending Rumble Commands

You can send rumble commands to the utility application using a simple TCP client. Here is an example in Python:

```py
import socket

def send_rumble_command(command):
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        s.connect(('127.0.0.1', 123456))
        s.sendall(command.encode())
        s.close()

# Example command: joystick_index low_freq high_freq duration
command = "0 32000 16000 5000"
send_rumble_command(command)
```

### Graceful Shutdown

The application handles graceful shutdown when receiving the SIGINT signal (Ctrl+C). It will close all open sockets and joysticks before exiting.

### Improvements

- Don't stop the process from starting if there are no joysticks connected but continually poll for joysticks being removed or added. Is there an update on change with SDL? I'll find out…
- Add proper logging 

### Further Support

I am not a low level system software developer so don't expect much more from me than what alreaady exists. It is purely a band-aid until someone fixes GLFW or ports the rumble.zig patch to a useful library.

### License
This project is licensed under the MIT License

You can use this README file to document the API and usage instructions for the SDL2 joystick rumble utility on GitHub
