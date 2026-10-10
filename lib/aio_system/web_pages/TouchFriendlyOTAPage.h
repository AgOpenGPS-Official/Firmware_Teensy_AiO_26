// Firmware_Teensy_AiO-New-Dawn is copyright 2025 by the AOG Group
// Firmware_Teensy_AiO-New-Dawn is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
// Firmware_Teensy_AiO-New-Dawn is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
// You should have received a copy of the GNU General Public License along with Firmware_Teensy_AiO-New-Dawn. If not, see <https://www.gnu.org/licenses/>.
// Like most Arduino code, portions of this are based on other open source Arduino code with a compatiable license.

// TouchFriendlyOTAPage.h
// Touch-optimized OTA firmware update page

#ifndef TOUCH_FRIENDLY_OTA_PAGE_H
#define TOUCH_FRIENDLY_OTA_PAGE_H

#include <Arduino.h>

const char TOUCH_FRIENDLY_OTA_PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no">
    <meta name="apple-mobile-web-app-capable" content="yes">
    <title>System Update - AiO v26</title>
    <link rel="stylesheet" href="/touch.css">
    <style>
        /* Additional styles specific to OTA update */
        .board-info {
            background: #e3f2fd;
            padding: 15px;
            border-radius: 8px;
            text-align: center;
            margin-bottom: 20px;
        }
        
        .board-info div {
            font-size: 18px;
            font-weight: 500;
            color: #1976d2;
            margin: 5px 0;
        }
        
        .warning-box {
            background: #ffebee;
            border: 2px solid #ef5350;
            padding: 15px;
            border-radius: 8px;
            margin-bottom: 20px;
            text-align: center;
            color: #c62828;
        }
        
        .warning-box strong {
            color: #c62828;
            font-size: 18px;
            display: block;
            margin-bottom: 5px;
        }
        
        
        .file-info {
            margin-top: 15px;
            padding: 15px;
            background: #e8f5e9;
            border-radius: 8px;
            font-family: monospace;
            word-break: break-all;
            display: none;
        }
        
        #feedback {
            font-family: monospace;
            background: #f5f5f5;
            padding: 15px;
            border-radius: 8px;
            margin: 20px 0;
            min-height: 120px;
            white-space: pre-wrap;
            font-size: 14px;
            border: 2px solid #bdc3c7;
        }
        
        .progress-track {
            height: 28px;
            background: #e0e0e0;
            border-radius: 14px;
            overflow: hidden;
            margin-top: 20px;
            display: none;
        }
        
        .progress-fill {
            height: 100%;
            width: 0%;
            background: #4caf50;
            transition: width 0.2s linear;
        }
        
        .progress-fill.busy {
            background: repeating-linear-gradient(45deg, #4caf50 0 14px, #66bb6a 14px 28px);
            background-size: 40px 28px;
            animation: progress-stripes 1s linear infinite;
        }
        
        .progress-fill.failed {
            background: #ef5350;
        }
        
        @keyframes progress-stripes {
            from { background-position: 0 0; }
            to { background-position: 40px 0; }
        }
        
        .progress-label {
            text-align: center;
            font-size: 18px;
            font-weight: 600;
            margin-top: 8px;
            display: none;
        }
        
        .upload-button {
            background: #4caf50;
        }
        
        .button-grid .upload-button {
            margin-top: 0;
        }
        
        .upload-button:active {
            background: #388e3c;
        }
        
        .upload-button:disabled {
            background: #95a5a6;
            opacity: 0.6;
        }
        
        .nav-buttons {
            display: grid;
            grid-template-columns: 1fr;
            gap: 15px;
            margin-bottom: 20px;
        }
        
        .button-grid {
            display: grid;
            grid-template-columns: 1fr 1fr;
            gap: 15px;
            margin-bottom: 20px;
        }
        
        .button-grid .touch-button,
        .button-grid label.touch-button {
            display: flex;
            align-items: center;
            justify-content: center;
            cursor: pointer;
            height: 60px;
            min-height: 60px;
            box-sizing: border-box;
            padding: 20px 25px;
            margin: 0;
            line-height: 1;
        }
        
        @media (max-width: 600px) {
            #feedback {
                font-size: 12px;
            }
        }
    </style>
    <script>
        let startVersion = '';
        
        function loadVersion() {
            fetch('/api/status')
            .then(response => response.json())
            .then(data => {
                if (data.version) {
                    startVersion = data.version;
                    document.getElementById('currentVersion').textContent = data.version;
                } else {
                    document.getElementById('currentVersion').textContent = 'Unknown';
                }
            })
            .catch(error => {
                console.error('Error loading version:', error);
                document.getElementById('currentVersion').textContent = 'Error';
            });
        }
        
        function loadPinStatus() {
            fetch('/api/ota/status')
            .then(response => response.json())
            .then(data => {
                const hint = document.getElementById('pinHint');
                if (!data.pinSet) {
                    hint.textContent = 'No OTA PIN is set. Updates are disabled until a PIN is set over the serial menu (press O).';
                    document.getElementById('uploadBtn').disabled = true;
                } else {
                    hint.textContent = 'Enter the OTA PIN set over the serial menu.';
                }
            })
            .catch(() => {});
        }
        
        window.onload = function() { loadVersion(); loadPinStatus(); };
        
        function displayFileName() {
            const fileInput = document.getElementById('file');
            const fileInfo = document.getElementById('fileInfo');
            const fileLabel = document.getElementById('fileLabel');
            const file = fileInput.files[0];
            
            if (file) {
                fileInfo.innerHTML = '<strong>Selected file:</strong> ' + file.name;
                fileInfo.style.display = 'block';
                fileLabel.textContent = 'File Selected: ' + file.name;
            } else {
                fileInfo.style.display = 'none';
                fileLabel.textContent = 'Choose Firmware File (.hex)';
            }
        }
        
        function setProgress(percent, label, state) {
            const track = document.getElementById('progressTrack');
            const fill = document.getElementById('progressFill');
            const text = document.getElementById('progressLabel');
            track.style.display = 'block';
            text.style.display = 'block';
            fill.style.width = percent + '%';
            fill.className = 'progress-fill' + (state ? ' ' + state : '');
            text.textContent = label;
        }
        
        function log(line) {
            document.getElementById('feedback').textContent += line + '\n';
        }
        
        function formatKB(bytes) {
            return Math.round(bytes / 1024).toLocaleString() + ' KB';
        }
        
        // Poll the device until it answers again after the reboot
        // confirmed: the device answered that it accepted the firmware
        function waitForReboot(confirmed) {
            const started = Date.now();
            let attempts = 0;
            setProgress(100, 'Step 3 of 3: Rebooting...', 'busy');
            log('Waiting for device to reboot...');
            
            function poll() {
                const elapsed = Math.round((Date.now() - started) / 1000);
                if (elapsed > 90) {
                    setProgress(100, 'Device did not come back', 'failed');
                    log('No answer after 90 s. Check the connection and reload this page.');
                    document.getElementById('uploadBtn').disabled = false;
                    return;
                }
                setProgress(100, 'Step 3 of 3: Rebooting... ' + elapsed + ' s', 'busy');
                
                const controller = new AbortController();
                const timer = setTimeout(() => controller.abort(), 2000);
                attempts++;
                fetch('/api/status?t=' + Date.now(), { signal: controller.signal, cache: 'no-store' })
                .then(response => response.json())
                .then(data => {
                    clearTimeout(timer);
                    const version = data.version || 'Unknown';
                    document.getElementById('currentVersion').textContent = version;
                    const unchanged = startVersion && version === startVersion;
                    if (unchanged && !confirmed) {
                        setProgress(100, 'Update not confirmed: still version ' + version, 'failed');
                        log('Device is back online with the same version and never confirmed the update. Try again.');
                    } else if (unchanged) {
                        setProgress(100, 'Update complete: version ' + version + ' (unchanged)', '');
                        log('Device is back online. Version is unchanged: ' + version);
                    } else {
                        setProgress(100, 'Update complete: version ' + version, '');
                        log('Device is back online. Version: ' + (startVersion ? startVersion + ' -> ' : '') + version);
                    }
                    document.getElementById('uploadBtn').disabled = false;
                })
                .catch(() => {
                    clearTimeout(timer);
                    setTimeout(poll, 1000);
                });
            }
            
            // Give the device time to start applying the update before the first poll
            setTimeout(poll, 3000);
        }
        
        function sendFirmware(name, content) {
            const uploadBtn = document.getElementById('uploadBtn');
            const total = content.length;
            let sentAll = false;
            let lastPercent = 0;
            
            log('File loaded: ' + name + ' (' + formatKB(total) + ')');
            setProgress(0, 'Step 1 of 3: Uploading... 0%', '');
            
            const xhr = new XMLHttpRequest();
            
            xhr.upload.addEventListener('progress', function(e) {
                if (!e.lengthComputable || sentAll) return;
                lastPercent = Math.floor((e.loaded / e.total) * 100);
                setProgress(lastPercent, 'Step 1 of 3: Uploading... ' + lastPercent + '% (' +
                            formatKB(e.loaded) + ' of ' + formatKB(e.total) + ')', '');
            });
            
            xhr.upload.addEventListener('load', function() {
                sentAll = true;
                log('Upload sent: ' + formatKB(total));
                setProgress(100, 'Step 2 of 3: Finishing upload and verifying...', 'busy');
            });
            
            xhr.addEventListener('load', function() {
                if (xhr.status === 200) {
                    log('Firmware accepted by device.');
                    waitForReboot(true);
                } else {
                    setProgress(lastPercent, 'Update failed', 'failed');
                    log('Update failed: ' + xhr.responseText);
                    uploadBtn.disabled = false;
                }
            });
            
            xhr.addEventListener('error', function() {
                if (sentAll) {
                    // The device can drop the connection as it reboots
                    log('Connection closed by device.');
                    waitForReboot(false);
                } else {
                    setProgress(lastPercent, 'Upload interrupted at ' + lastPercent + '%', 'failed');
                    log('Connection lost during upload. Check the connection and try again.');
                    uploadBtn.disabled = false;
                }
            });
            
            xhr.open('POST', '/api/ota/upload');
            xhr.setRequestHeader('Content-Type', 'text/plain');
            xhr.setRequestHeader('X-OTA-PIN', document.getElementById('otaPin').value);
            xhr.send(content);
        }
        
        function uploadFile() {
            const fileInput = document.getElementById('file');
            const file = fileInput.files[0];
            
            if (!file) {
                alert('Please select a firmware file');
                return false;
            }
            
            if (!file.name.endsWith('.hex')) {
                alert('Please select a .hex firmware file');
                return false;
            }
        
            if (!document.getElementById('otaPin').value) {
                alert('Enter the OTA PIN');
                return false;
            }
        
            document.getElementById('feedback').textContent = '';
            document.getElementById('uploadBtn').disabled = true;
            
            // Read file content
            const reader = new FileReader();
            reader.onload = function(e) {
                sendFirmware(file.name, e.target.result);
            };
            reader.readAsText(file);
            return false;
        }
    </script>
</head>
<body>
    <div class="container">
        <h1>System Update</h1>
        
        <div class="nav-buttons">
            <button type="button" class="touch-button" style="background: #7f8c8d;" 
                    onclick="window.location.href='/'">
                Back to Home
            </button>
        </div>
        
        <div class="card">
            <div class="button-grid">
                <label for="file" class="touch-button">
                    Choose Firmware
                </label>
                <button type="button" id="uploadBtn" class="touch-button upload-button" onclick="uploadFile()">
                    Upload Firmware
                </button>
            </div>
            
            <input type="password" id="otaPin" placeholder="OTA PIN" maxlength="16" autocomplete="off"
                   style="width:100%; padding:14px; margin-top:12px; font-size:18px; box-sizing:border-box;">
            <div id="pinHint" class="file-info"></div>
            
            <input type="file" id="file" name="firmware" accept=".hex" onchange="displayFileName()" style="display: none;">
            
            <div id="fileInfo" class="file-info"></div>
            
            <div id="progressTrack" class="progress-track"><div id="progressFill" class="progress-fill"></div></div>
            <div id="progressLabel" class="progress-label"></div>
            
            <div id="feedback"></div>
        </div>
        
        <div class="board-info">
            <div>Board Type: Teensy 4.1</div>
            <div>Current Version: <span id="currentVersion">Loading...</span></div>
        </div>
        
        <div class="warning-box">
            <strong>⚠️ Warning</strong>
            Incorrect firmware can brick your device. Only upload firmware built for Teensy 4.1.
        </div>
        
    </div>
</body>
</html>
)rawliteral";

#endif // TOUCH_FRIENDLY_OTA_PAGE_H