# Threads Demo (C++17)

A simple CLI program to demonstrate multithreading and synchronization.

## Features
- 3 worker threads (`std::thread`, `std::atomic`, `std::mutex`)
- Safe console output with a mutex
- Interactive menu:
  1. Show thread status  
  2. Start/stop a thread  
  3. Change message interval (≥100 ms)  
  4. Start all threads  
  5. Pause all threads  
  0. Exit  
