# AiO v26 Architecture Overview

This document provides a high-level overview of the AiO v26 system architecture and its key components.

## System Overview

AiO v26 is a modular agricultural control system built on the Teensy 4.1 platform. It integrates GPS/GNSS navigation, automated steering, section control, and network communication to work with AgOpenGPS.

## Core Architecture Principles

- **Modular Design**: Each subsystem is self-contained with clear interfaces
- **Event-Driven Communication**: Uses PGN (Parameter Group Number) messages for inter-module communication
- **Real-Time Processing**: Critical control loops run at consistent intervals
- **Network-First**: Built around QNEthernet for reliable UDP communication

## See the docs directory for complete documentation.
