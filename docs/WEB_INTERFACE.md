# Web Interface Documentation

This document describes the web interface implementation in AiO v26, including the lightweight HTTP server, WebSocket communication, and available configuration pages.

## Overview

AiO v26 includes a custom lightweight web server that provides:
- System configuration without external dependencies
- Real-time telemetry via WebSocket
- Responsive design for desktop and mobile
- OTA firmware updates
- Event logging and debugging

## Architecture

### SimpleWebManager

The core web server implementation:

```cpp
SimpleWebManager
    ├── SimpleHTTPServer (Port 80)
    ├── SimpleWebSocket (Real-time data)
    ├── Page Templates (Embedded HTML)
    └── API Endpoints (RESTful)
```

### Key Features

- **No External Dependencies**: Custom HTTP/WebSocket implementation
- **Embedded Resources**: HTML/CSS/JS compiled into firmware
- **Non-blocking**: Event-driven architecture
- **Memory Efficient**: Minimal RAM usage
- **Template System**: Reusable page components

## Available Pages

### Home Page (`/`)

System overview and navigation:
- Current status display
- Quick links to settings
- System information
- LED status indicators

### Network Settings (`/network`)

Network configuration interface:
- IP address settings
- DHCP/Static mode selection
- Subnet mask configuration
- Gateway settings
- MAC address display

### Device Settings (`/device_settings`)

Hardware configuration:
- Motor type selection
- Sensor calibration
- Pin assignments
- Output configuration
- System parameters

### Analog/Work Switch (`/analog_work_switch`)

Sensor calibration and testing:
- WAS calibration wizard
- Current sensor zero
- Work switch setup
- Real-time value display
- Threshold adjustment

### Event Logger (`/event_logger`)

Real-time system monitoring:
- WebSocket-based log streaming
- Severity level filtering
- Source-based filtering
- Export capabilities
- Clear log function

### OTA Update (`/ota`)

Firmware update interface:
- Drag-and-drop upload
- Progress indication
- Automatic reboot
- Version display
- Update history

## WebSocket Protocol

### Connection

WebSocket endpoint: `ws://[device-ip]/ws`

### Message Format

All messages use JSON format:

```json
{
  "type": "message_type",
  "data": { ... }
}
```

### Message Types

#### Status Updates
```json
{
  "type": "status",
  "data": {
    "steer": {
      "angle": 15.5,
      "enabled": true,
      "current": 2.3
    },
    "gps": {
      "lat": 51.123456,
      "lon": -114.123456,
      "fix": "RTK Fixed"
    },
    "network": {
      "connected": true,
      "ip": "192.168.5.126"
    }
  }
}
```

#### Configuration Changes
```json
{
  "type": "config",
  "data": {
    "parameter": "p_gain",
    "value": 120
  }
}
```

#### Event Logs
```json
{
  "type": "log",
  "data": {
    "timestamp": 1234567890,
    "level": "INFO",
    "source": "AutosteerProcessor",
    "message": "Autosteer engaged"
  }
}
```

#### Telemetry Stream
```json
{
  "type": "telemetry",
  "data": {
    "was": 2048,
    "current": 512,
    "encoder": 1000,
    "pwm": 128
  }
}
```

## API Endpoints

### GET Endpoints

#### `/api/status`
Returns current system status:
```json
{
  "version": "1.0.0-beta",
  "uptime": 123456,
  "processor": "Teensy 4.1",
  "modules": ["steer", "gps", "imu"]
}
```

#### `/api/config`
Returns current configuration:
```json
{
  "network": {
    "ip": "192.168.5.126",
    "dhcp": false
  },
  "steer": {
    "p_gain": 120,
    "max_pwm": 255
  }
}
```

#### `/api/sensors`
Returns sensor readings:
```json
{
  "was": {
    "raw": 2048,
    "angle": 15.5
  },
  "current": {
    "raw": 512,
    "amps": 2.3
  }
}
```

### POST Endpoints

#### `/api/config`
Update configuration:
```json
POST /api/config
Content-Type: application/json

{
  "steer": {
    "p_gain": 150
  }
}
```

#### `/api/calibrate`
Trigger calibration:
```json
POST /api/calibrate
Content-Type: application/json

{
  "sensor": "was",
  "action": "center"
}
```

#### `/api/reboot`
Restart the system:
```json
POST /api/reboot
```

## Page Template System

### Common Styles

All pages include common CSS from `CommonStyles.h`:
- Responsive grid layout
- Mobile-friendly navigation
- Consistent color scheme
- Form styling
- Status indicators

### Template Structure

```html
<!DOCTYPE html>
<html>
<head>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <title>AiO v26 - Page Title</title>
    <style>
        /* CommonStyles included here */
    </style>
</head>
<body>
    <div class="header">
        <h1>Page Title</h1>
    </div>
    <div class="nav">
        <!-- Navigation links -->
    </div>
    <div class="content">
        <!-- Page content -->
    </div>
</body>
</html>
```

### JavaScript Integration

Pages use vanilla JavaScript for:
- WebSocket communication
- Form validation
- Dynamic updates
- AJAX requests
- Real-time displays

## Implementation Details

### Memory Management

- HTML/CSS/JS stored in PROGMEM
- Chunked response sending
- Minimal heap allocation
- Connection pooling

### Security Considerations

- No authentication (local network only)
- Input validation on all forms
- XSS prevention in templates
- CSRF protection via tokens

### Performance Optimization

- Gzip compression support
- Cache headers for static content
- Minimal JavaScript libraries
- Efficient WebSocket protocol

## Customization

### Adding New Pages

1. Create page template in `web_pages/`
2. Register route in SimpleWebManager
3. Implement handler function
4. Add navigation link

### Modifying Existing Pages

1. Edit template in `web_pages/`
2. Maintain placeholder system
3. Test on multiple devices
4. Verify WebSocket compatibility

### Styling Changes

1. Update CommonStyles.h
2. Keep mobile compatibility
3. Test color contrast
4. Maintain consistency

## WebSocket Client Example

```javascript
// Connect to WebSocket
const ws = new WebSocket('ws://' + window.location.host + '/ws');

// Handle connection
ws.onopen = function() {
    console.log('WebSocket connected');
    // Subscribe to telemetry
    ws.send(JSON.stringify({
        type: 'subscribe',
        stream: 'telemetry'
    }));
};

// Handle messages
ws.onmessage = function(event) {
    const msg = JSON.parse(event.data);
    switch(msg.type) {
        case 'telemetry':
            updateDisplay(msg.data);
            break;
        case 'log':
            appendLog(msg.data);
            break;
    }
};

// Send configuration
function updateConfig(param, value) {
    ws.send(JSON.stringify({
        type: 'config',
        data: {
            parameter: param,
            value: value
        }
    }));
}
```

## Troubleshooting

### Connection Issues

**Cannot access web interface**:
- Verify IP address
- Check network connection
- Ensure port 80 is not blocked
- Try different browser

**WebSocket disconnects**:
- Check network stability
- Monitor connection count
- Verify message size
- Check timeout settings

### Display Issues

**Page not rendering correctly**:
- Clear browser cache
- Check JavaScript console
- Verify HTML validity
- Test different browsers

**Real-time updates not working**:
- Check WebSocket connection
- Verify message format
- Monitor network traffic
- Check browser compatibility

## Best Practices

### Development
1. Test on multiple devices
2. Minimize page size
3. Use semantic HTML
4. Implement error handling

### Deployment
1. Test all functionality
2. Verify mobile layout
3. Check performance
4. Monitor memory usage

### Maintenance
1. Keep templates simple
2. Document API changes
3. Version API endpoints
4. Log client errors