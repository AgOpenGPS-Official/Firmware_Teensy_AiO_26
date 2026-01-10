# How to Add New Translation Pages

This guide explains how to create and add new translation pages to the AiO v26 web interface. It's written for team members and developers of all skill levels.

## Overview

The AiO v26 web interface supports multiple languages. Currently, we have:
- English (EN) - Primary language for all pages
- German (DE) - Example translation for the homepage only

## Prerequisites

- Basic text editor (VSCode, Notepad++, etc.)
- PlatformIO installed (for building/testing)
- Basic understanding of HTML
- Git for version control

## File Structure

Translation files are located in:
```
lib/aio_system/web_pages/
├── en/           # English pages (primary)
│   ├── home.html
│   ├── network.html
│   ├── tools.html
│   └── ota.html
└── de/           # German translations
    └── home.html # Example translation
```

## Steps to Add a New Translation

### 1. Create Language Directory

If the language directory doesn't exist, create it:
```bash
mkdir -p lib/aio_system/web_pages/[language_code]
```

Example language codes:
- `fr` - French
- `es` - Spanish
- `it` - Italian
- `nl` - Dutch

### 2. Copy English Template

Copy the English page you want to translate:
```bash
cp lib/aio_system/web_pages/en/home.html lib/aio_system/web_pages/[language_code]/home.html
```

### 3. Translate the Content

Edit the copied file and translate:
- Page title in `<title>` tag
- All visible text content
- Button labels
- Status messages
- Form labels

**Important Translation Guidelines:**
- Do NOT translate:
  - HTML tags and attributes
  - CSS classes and IDs
  - JavaScript code
  - URL endpoints (e.g., `/api/status`)
  - Technical identifiers (e.g., `id="ipOctet1"`)
  
- DO translate:
  - All user-visible text
  - Placeholder text in input fields
  - Alert and confirmation messages
  - Status indicators

### 4. Update WebManager.cpp

**File location:** `lib/aio_system/WebManager.cpp`

Add the new translation to the web server routes. Look for the section with existing routes (around line 100-200):

```cpp
// In WebManager::begin() function
// Look for this section with existing routes:

// Existing German homepage
server.on("/de", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send_P(200, "text/html", home_de_html, processor);
});

// Add your new translation below the existing ones
// Replace [language_code] with your actual code (fr, es, etc.)
server.on("/[language_code]", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send_P(200, "text/html", home_[language_code]_html, processor);
});
```

**Note:** The variable name `home_[language_code]_html` must match exactly what the build system generates.

### 5. Include the Translation File

Add the include statement at the top of WebManager.cpp:
```cpp
// Existing includes
#include "web_pages/en/home.html.h"
#include "web_pages/de/home.html.h"

// Add your translation
#include "web_pages/[language_code]/home.html.h"
```

### 6. Build the Project

The build system automatically converts .html files to .h header files during compilation:

```bash
# In the project root directory
~/.platformio/penv/bin/pio run -e teensy41
```

The build process:
- Finds all .html files
- Escapes special characters
- Converts to a C string array
- Creates .h files with names like `home_[language_code]_html`
- Places them next to the .html files

**Troubleshooting Build Errors:**
- If you get "undefined reference to `home_[language_code]_html`", check:
  - The include path is correct
  - The language code matches exactly
  - The .html file exists in the right location
- Run a clean build if needed: `pio run -t clean -e teensy41`

## Complete Example: Adding French Homepage

Let's walk through adding a French translation step by step:

### Step 1: Create the directory
```bash
cd /path/to/AiO_New_Dawn
mkdir -p lib/aio_system/web_pages/fr
```

### Step 2: Copy the English template
```bash
cp lib/aio_system/web_pages/en/home.html lib/aio_system/web_pages/fr/home.html
```

### Step 3: Open and translate the file
Open `lib/aio_system/web_pages/fr/home.html` in your editor and translate the content:

```html
<!-- Original English -->
<title>AiO v26</title>
<h1>AiO v26 Status</h1>
<h2>Network Information</h2>
<button onclick="location.reload()">Refresh</button>

<!-- Translate to French -->
<title>AiO Nouvelle Aube</title>
<h1>État AiO Nouvelle Aube</h1>
<h2>Informations Réseau</h2>
<button onclick="location.reload()">Actualiser</button>
```

### Step 4: Edit WebManager.cpp
Open `lib/aio_system/WebManager.cpp` and make two changes:

**A. Add the include at the top (around line 20-30):**
```cpp
#include "web_pages/en/home.html.h"
#include "web_pages/de/home.html.h"
#include "web_pages/fr/home.html.h"  // ADD THIS LINE
```

**B. Add the route in the begin() function (around line 150-200):**
```cpp
// German homepage
server.on("/de", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send_P(200, "text/html", home_de_html, processor);
});

// French homepage - ADD THESE LINES
server.on("/fr", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send_P(200, "text/html", home_fr_html, processor);
});
```

### Step 5: Build and test
```bash
~/.platformio/penv/bin/pio run -e teensy41
```

### Step 6: Upload and verify
After uploading, navigate to: `http://[your-device-ip]/fr`

## Testing Your Translation

1. Build the project:
```bash
pio run -e teensy41
```

2. Upload to device and access:
```
http://[device-ip]/[language_code]
```

Example: `http://192.168.1.126/fr`

## Best Practices

1. **Work with Native Speakers**: Always have native speakers review translations
2. **Maintain Consistency**: Use consistent terminology across all pages
3. **Keep Technical Terms**: Some technical terms may not need translation
4. **Test Layout**: Some languages use more characters - test that UI elements still fit
5. **Cultural Considerations**: Be aware of cultural differences in date/time formats, number formats, etc.

## Adding Language Selection

To add a language selector to the interface:

1. Add to each page's HTML:
```html
<div class="language-selector">
    <a href="/en">EN</a> | 
    <a href="/de">DE</a> | 
    <a href="/fr">FR</a>
</div>
```

2. Style appropriately:
```css
.language-selector {
    position: absolute;
    top: 10px;
    right: 10px;
}
```

## Common Issues and Solutions

### Problem: "undefined reference to `home_xx_html`"
**Solution:** 
- Check that the include path matches exactly
- Verify the .html file exists
- Make sure the language code is consistent everywhere
- Try a clean build: `pio run -t clean -e teensy41`

### Problem: Special characters show incorrectly
**Solution:**
- Save your HTML file as UTF-8 encoding
- The build system will handle the conversion

### Problem: Changes don't appear after upload
**Solution:**
- Clear your browser cache (Ctrl+F5 or Cmd+Shift+R)
- Check you're accessing the correct URL
- Verify the upload was successful

## Quick Reference

### Language Codes
- `en` - English
- `de` - German (Deutsch)
- `fr` - French (Français)
- `es` - Spanish (Español)
- `it` - Italian (Italiano)
- `nl` - Dutch (Nederlands)
- `pt` - Portuguese (Português)
- `pl` - Polish (Polski)

### Files to Modify
1. Create: `lib/aio_system/web_pages/[lang]/[page].html`
2. Edit: `lib/aio_system/WebManager.cpp` (2 places)
3. Build: `pio run -e teensy41`

### Testing URLs
- English: `http://[device-ip]/` or `http://[device-ip]/en`
- German: `http://[device-ip]/de`
- Your language: `http://[device-ip]/[language-code]`

## Notes

- Currently, only the homepage has a German translation as an example
- The system is designed to be easily extensible for full multi-language support
- When in doubt, follow the pattern of the existing German translation
- Always test translations in actual use to ensure context is correct
- Don't hesitate to ask team members for help!