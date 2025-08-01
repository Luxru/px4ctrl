<a id="readme-top"></a>
<!-- PROJECT LOGO -->
<br />
<div align="center">
  <a href="https://github.com/CQU-UISC/px4ctrl_client">
    <img src="images/logo.png" alt="Logo" width="80" height="80">
  </a>
  <h3 align="center">UISC Lab Px4Ctrl</h3>
  <p align="center">
    Px4Ctrl
  </p>
  <img align="center" src=https://img.shields.io/badge/license-GPL--3.0-blue  alt="license"/>
</div>

## About
A Px4 controller implemented based on Mavros.

## Architecture

**Outer Loop:** Switches control mode based on current state estimation or user command.

**Control Modes:**
1.  **Position Controller:** The common mode, used when 6-DoF estimation is available.
2.  **Command Controller:** For control via an external Controller.

## Prerequisites
- [spdlog](https://github.com/gabime/spdlog)
- [C++ 20](https://en.cppreference.com/w/cpp/compiler_support)

## Installation

```bash
git clone https://github.com/CQU-UISC/px4ctrl.git
cd px4ctrl
git submodule update --init --recursive
mkdir build && cd build
cmake ..
make -j4
```

## Usage

TODO

## Roadmap

  - [ ] Add safety control methods (for situations like odom timeout, emergency landing, etc.)
  - [x] Implement using DDS
  - [x] Move ZMQ proxy
  - [x] Bug exists: Attitude control fails in force hover mode.
  - [x] Reset throttle estimation upon re-takeoff.

## Contact

Xu Lu - lux@cqu.edu.cn

## Acknowledgments

  * [ZJU FastLab](https://github.com/ZJU-FAST-Lab)
  * [UZH Robotics and Perception Group](https://github.com/uzh-rpg)