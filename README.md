# CPUAffinity

**CPUAffinity** is a Qt-based Windows utility for inspecting and editing the CPU affinity of running processes.  
It provides a graphical interface to select a process, view detailed information, and adjust how many CPU cores the process is allowed to use.

---

## Features

- **Process Picker**  
  Browse all currently running applications and select a process to inspect.

- **Process Info Panel**  
  Displays detailed information about the selected process:
  - Name, PID, Window Title
  - Executable Path
  - Start Time
  - CPU time
  - Memory usage (Working Set, Private, Paged)
  - Threads, Handles
  - Responding status
  - Number of cores currently assigned

- **Editor Panel**  
  - Adjust the number of CPU cores assigned to the selected process.
  - Save your configuration to a JSON file.
  - Load configurations back into the editor (coming soon).
  - Apply the configuration to the process immediately.

- **Config Management**  
  - Save and Save As… store your affinity settings in a JSON file.
  - Load (planned) will restore saved settings.
  - Config files are portable and human-readable.

---

## Requirements

- **Operating System**: Windows 10/11  
- **Qt Version**: Qt 6.x (built and tested with Qt 6)  
- **Compiler**: MSVC (via Visual Studio 2022)  
- **Build System**: CMake or Qt VS Tools
