// Firmware_Teensy_AiO-New-Dawn is copyright 2025 by the AOG Group
// Firmware_Teensy_AiO-New-Dawn is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
// Firmware_Teensy_AiO-New-Dawn is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
// You should have received a copy of the GNU General Public License along with Firmware_Teensy_AiO-New-Dawn. If not, see <https://www.gnu.org/licenses/>.
// Like most Arduino code, portions of this are based on other open source Arduino code with a compatiable license.

// TouchFriendlyDNSAliasPage.h
// mDNS alias configuration page - allows the user to define up to 4 extra shortnames
// (e.g. "board") under which the AiO board is reachable as board.local.

#ifndef TOUCH_FRIENDLY_DNS_ALIAS_PAGE_H
#define TOUCH_FRIENDLY_DNS_ALIAS_PAGE_H

#include <Arduino.h>

const char TOUCH_FRIENDLY_DNS_ALIAS_PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no">
    <meta name="apple-mobile-web-app-capable" content="yes">
    <title>mDNS Names - AiO v26</title>
    <link rel="stylesheet" href="/touch.css">
    <style>
        .alias-row {
            display: flex;
            align-items: center;
            gap: 12px;
            padding: 12px 0;
            border-bottom: 1px solid #ecf0f1;
        }
        .alias-row:last-child { border-bottom: none; }
        .alias-label {
            min-width: 80px;
            font-weight: 500;
            color: #7f8c8d;
            font-size: 17px;
        }
        .alias-input {
            flex: 1;
            padding: 12px 16px;
            font-size: 17px;
            border: 2px solid #bdc3c7;
            border-radius: 8px;
            font-family: monospace;
            text-transform: lowercase;
        }
        .alias-input:focus { border-color: #3498db; outline: none; }
        .alias-suffix {
            font-size: 17px;
            color: #95a5a6;
            white-space: nowrap;
        }
        .info-box {
            background: #eaf4fb;
            border-left: 4px solid #3498db;
            border-radius: 6px;
            padding: 14px 16px;
            margin-bottom: 20px;
            font-size: 15px;
            color: #2c3e50;
            line-height: 1.5;
        }
        .status-msg {
            display: none;
            padding: 10px 16px;
            border-radius: 8px;
            margin-top: 12px;
            font-size: 16px;
        }
        .status-msg.ok  { background:#d4edda; color:#155724; display:block; }
        .status-msg.err { background:#f8d7da; color:#721c24; display:block; }
        .section-title {
            font-size: 14px;
            font-weight: 600;
            text-transform: uppercase;
            letter-spacing: 1px;
            color: #95a5a6;
            margin: 20px 0 8px 0;
        }
        .fixed-alias-list {
            background: #f8f9fa;
            border-radius: 8px;
            padding: 10px 16px;
        }
        .fixed-alias-list span {
            display: inline-block;
            background: #d5e8fb;
            color: #1565c0;
            border-radius: 4px;
            padding: 4px 10px;
            margin: 4px;
            font-family: monospace;
            font-size: 15px;
        }
        .nav-buttons {
            display: grid;
            grid-template-columns: 1fr 1fr;
            gap: 15px;
            margin-bottom: 20px;
        }
    </style>
</head>
<body>
    <div class="container">
        <h1>mDNS Names</h1>

        <div class="nav-buttons">
            <button type="button" class="touch-button" style="background: #7f8c8d;"
                    onclick="window.location.href='/'">
                Back to Home
            </button>
            <button type="button" class="touch-button" onclick="saveAliases()">
                Apply Changes
            </button>
        </div>

        <div class="card">
            <div class="info-box">
                Aliases are shortnames (without <code>.local</code>) under which <b>this AiO board</b> is
                reachable in the browser via <code>.local</code>, e.g. <code>board.local</code> or <code>lenkung.local</code>.<br>
                Leave fields empty to disable.
            </div>

            <div class="section-title">Fixed system names (not editable)</div>
            <div class="fixed-alias-list">
                <span title="Always serves the Teensy web interface">aio.local</span>
                <span title="Always forwards to the ESP32 WiFi bridge">wifi.local &rarr; ESP32</span>
            </div>

            <div class="section-title" style="margin-top:20px;">Editable aliases &mdash; pre-filled: <code>gps.local</code>, <code>steer.local</code></div>
            <div id="alias-form">
                <div class="alias-row">
                    <span class="alias-label">Alias 1</span>
                    <input class="alias-input" type="text" id="a0" maxlength="11"
                           placeholder="e.g. board" pattern="[a-zA-Z0-9\-]*">
                    <span class="alias-suffix">.local</span>
                </div>
                <div class="alias-row">
                    <span class="alias-label">Alias 2</span>
                    <input class="alias-input" type="text" id="a1" maxlength="11"
                           placeholder="e.g. lenkung" pattern="[a-zA-Z0-9\-]*">
                    <span class="alias-suffix">.local</span>
                </div>
                <div class="alias-row">
                    <span class="alias-label">Alias 3</span>
                    <input class="alias-input" type="text" id="a2" maxlength="11"
                           placeholder="" pattern="[a-zA-Z0-9\-]*">
                    <span class="alias-suffix">.local</span>
                </div>
                <div class="alias-row">
                    <span class="alias-label">Alias 4</span>
                    <input class="alias-input" type="text" id="a3" maxlength="11"
                           placeholder="" pattern="[a-zA-Z0-9\-]*">
                    <span class="alias-suffix">.local</span>
                </div>
            </div>
            <div id="status-msg" class="status-msg"></div>
        </div>

        <div class="card">
            <h2 style="margin-top:0;">How it works</h2>
            <p style="font-size:15px;color:#555;line-height:1.6;">
                This board uses <b>.local</b> host names for the Teensy page, WiFi bridge,
                aliases, and module proxy routing. The HTTP server evaluates the <code>Host</code>
                header to decide what to serve:<br><br>
                &bull; <b>aio.local</b> &rarr; Teensy web interface (fixed)<br>
                &bull; <b>wifi.local</b> &rarr; ESP32 WiFi bridge page (fixed)<br>
                &bull; <b>gps.local, steer.local, &hellip;</b> &rarr; Teensy web interface (editable defaults above)<br>
                &bull; <b>Module names</b> (e.g. sc.local) &rarr; forwarded via ESP32 to the WiFi module<br>
                &bull; <b>Direct IP</b> (192.168.5.126) &rarr; always serves the Teensy web interface
            </p>
        </div>
    </div>

    <script>
    (function() {
        // Load current aliases from API
        fetch('/api/dns-alias/config')
            .then(r => r.json())
            .then(d => {
                for (var i = 0; i < 4; i++) {
                    var el = document.getElementById('a' + i);
                    if (el && d.aliases && d.aliases[i] !== undefined) {
                        el.value = d.aliases[i];
                    }
                }
            })
            .catch(function() { showStatus('Could not load current aliases.', false); });
    })();

    function saveAliases() {
        var aliases = [];
        for (var i = 0; i < 4; i++) {
            var v = (document.getElementById('a' + i).value || '').toLowerCase().replace(/[^a-z0-9\-]/g, '');
            aliases.push(v);
        }
        fetch('/api/dns-alias/config', {
            method: 'POST',
            headers: {'Content-Type': 'application/json'},
            body: JSON.stringify({aliases: aliases})
        })
        .then(r => r.json())
        .then(d => {
            if (d.status === 'ok') {
                showStatus('Aliases saved. Changes take effect immediately.', true);
            } else {
                showStatus('Error: ' + (d.message || 'unknown'), false);
            }
        })
        .catch(function() { showStatus('Could not save aliases (network error).', false); });
    }

    function showStatus(msg, ok) {
        var el = document.getElementById('status-msg');
        el.textContent = msg;
        el.className = 'status-msg ' + (ok ? 'ok' : 'err');
    }
    </script>
</body>
</html>
)rawliteral";

#endif // TOUCH_FRIENDLY_DNS_ALIAS_PAGE_H
