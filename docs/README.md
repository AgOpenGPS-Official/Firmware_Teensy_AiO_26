# AiO v26 Documentation

Welcome to the AiO v26 documentation. This folder contains comprehensive documentation for understanding, configuring, and developing with the AiO v26 agricultural control system.

## Documentation Overview

### Core Documentation

1. **[ARCHITECTURE_OVERVIEW.md](ARCHITECTURE_OVERVIEW.md)**
   - System architecture and design principles
   - Component overview and relationships
   - Data flow and communication patterns
   - Performance considerations

2. **[CLASS_INTERACTION_DIAGRAM.md](CLASS_INTERACTION_DIAGRAM.md)**
   - Visual representation of class relationships
   - Design patterns used in the codebase
   - Module initialization order
   - PGN message registration details

3. **[HARDWARE_AND_PIN_MANAGEMENT.md](HARDWARE_AND_PIN_MANAGEMENT.md)**
   - Pin assignments for Teensy 4.1 and Little Dawn
   - Resource management system
   - Hardware abstraction layer
   - Conflict resolution and debugging

4. **[COMMUNICATION_AND_NETWORKING.md](COMMUNICATION_AND_NETWORKING.md)**
   - Network configuration and protocols
   - Serial communication interfaces
   - CAN bus implementation
   - I2C device management

5. **[MOTOR_DRIVERS_AND_SENSORS.md](MOTOR_DRIVERS_AND_SENSORS.md)**
   - Supported motor driver types
   - Sensor interfaces and calibration
   - Control loop implementation
   - Safety features and troubleshooting

6. **[WEB_INTERFACE.md](WEB_INTERFACE.md)**
   - Web server architecture
   - Available configuration pages
   - WebSocket protocol
   - API endpoints and customization

7. **[LED_Status_Guide.md](LED_Status_Guide.md)**
   - Front panel LED meanings
   - Color codes and patterns
   - Troubleshooting with LEDs
   - Visual feedback system

8. **[QUICK_REFERENCE.md](QUICK_REFERENCE.md)**
   - Essential commands and shortcuts
   - Pin assignments reference
   - Common issues and solutions
   - Performance metrics

## Getting Started

If you're new to AiO v26, we recommend reading the documentation in this order:

1. Start with [ARCHITECTURE_OVERVIEW.md](ARCHITECTURE_OVERVIEW.md) to understand the system
2. Review [LED_Status_Guide.md](LED_Status_Guide.md) to understand status indicators
3. Check [QUICK_REFERENCE.md](QUICK_REFERENCE.md) for common tasks
4. Dive into specific topics as needed

## For Developers

If you're planning to modify or extend AiO v26:

1. Study [CLASS_INTERACTION_DIAGRAM.md](CLASS_INTERACTION_DIAGRAM.md) for code structure
2. Review [HARDWARE_AND_PIN_MANAGEMENT.md](HARDWARE_AND_PIN_MANAGEMENT.md) before adding hardware
3. Understand [COMMUNICATION_AND_NETWORKING.md](COMMUNICATION_AND_NETWORKING.md) for protocol details
4. Follow patterns in [MOTOR_DRIVERS_AND_SENSORS.md](MOTOR_DRIVERS_AND_SENSORS.md) for new drivers

## Additional Resources

- **Knowledge Base**: Check the `/knowledge` folder for implementation guides
- **Source Code**: Well-commented code in `/lib` and `/src`
- **Web Interface**: Access device at `http://192.168.5.126` for configuration
- **Serial Menu**: Press `?` at boot for interactive debugging

## Documentation Standards

When contributing to documentation:

- Use clear, concise language
- Include code examples where helpful
- Keep formatting consistent
- Update the table of contents
- Test all commands and procedures

## Version

This documentation is current as of version 1.0.0-beta. Check the git history for the latest updates.

## Questions?

If you can't find what you need:

1. Check the source code comments
2. Use the serial menu for debugging
3. Review the web interface help
4. Examine the `/knowledge` folder