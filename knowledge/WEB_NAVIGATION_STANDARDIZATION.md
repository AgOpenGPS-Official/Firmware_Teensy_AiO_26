# Web Navigation Standardization

## Summary of Changes

This document describes the standardization of navigation buttons across all web pages in the AiO v26 firmware.

## Navigation Layout Standard

All pages now follow this consistent button layout:
1. **Home** (leftmost) - Blue button that returns to the home page
2. **Apply Changes** - Green primary action button for saving/applying changes
3. **Additional Actions** - Any page-specific buttons (Reboot, Clear, etc.)

## Button Styling

Added new CSS classes in `CommonStyles.h`:
- `.btn` - Base button class
- `.btn-home` - Blue home button
- `.btn-primary` - Green primary action button  
- `.btn-warning` - Orange warning actions (e.g., Reboot)
- `.btn-danger` - Red dangerous actions
- `.btn-secondary` - Gray secondary actions
- `.nav-buttons` - Container for button groups with flexbox layout

## Pages Updated

1. **Event Logger Page** (English & German)
   - Before: "Save Configuration", "Back to Home"
   - After: "Home", "Apply Changes"

2. **Network Settings Page**
   - Before: "Save", "Reboot", "Cancel"
   - After: "Home", "Apply Changes", "Reboot"

3. **Device Settings Page**
   - Before: "Save Settings", "Back to Home"
   - After: "Home", "Apply Changes"

4. **OTA Update Page**
   - Before: "Upload Firmware", "Cancel"
   - After: "Home", "Apply Update"

5. **WAS Demo Page**
   - Before: "Back to Home"
   - After: "Home"

## Benefits

- Consistent user experience across all pages
- Users always know where to find the home button (leftmost position)
- Clear distinction between navigation and actions
- Improved visual hierarchy with color-coded button types
- Responsive flexbox layout for mobile devices