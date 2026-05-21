#include "r2d2_webserver.h"
#include <esp_http_server.h>
#include <esp_log.h>
#include "r2d2_audio.h"
#include "r2d2_display.h"
#include "r2d2_modes.h"
#include <time.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <unistd.h>
#include <ctype.h>

static const char *TAG = "R2D2_WEBSERVER";

/* Modern, Star Wars themed glassmorphism UI for R2-D2 Control Panel */
static const char *dashboard_html =
"<!DOCTYPE html>\n"
"<html lang=\"en\">\n"
"<head>\n"
"    <meta charset=\"UTF-8\">\n"
"    <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\n"
"    <title>R2-D2 Astromech Dashboard</title>\n"
"    <link href=\"https://fonts.googleapis.com/css2?family=Orbitron:wght@500;800&family=Inter:wght@400;600&display=swap\" rel=\"stylesheet\">\n"
"    <style>\n"
"        :root {\n"
"            --primary: #ffe81f;\n"
"            --accent: #ff3333;\n"
"            --bg: #000000;\n"
"            --panel: rgba(20, 20, 20, 0.85);\n"
"        }\n"
"        body {\n"
"            margin: 0; padding: 0; font-family: 'Inter', sans-serif;\n"
"            background: var(--bg); color: var(--primary);\n"
"            min-height: 100vh; overflow-y: auto;\n"
"            display: flex; flex-direction: column; align-items: center;\n"
"            perspective: 800px;\n"
"            padding-bottom: 60px;\n"
"            box-sizing: border-box;\n"
"            cursor: url('data:image/svg+xml;utf8,<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"32\" height=\"32\" viewBox=\"0 0 32 32\"><rect x=\"14\" y=\"2\" width=\"4\" height=\"20\" fill=\"%2300f0ff\" rx=\"2\"/><rect x=\"13\" y=\"22\" width=\"6\" height=\"8\" fill=\"silver\" rx=\"1\"/></svg>') 16 16, auto;\n"
"        }\n"
"        .stars {\n"
"            position: absolute; top: 0; left: 0; right: 0; bottom: 0; z-index: -2;\n"
"            background-image: \n"
"                radial-gradient(1px 1px at 20px 30px, #fff, rgba(0,0,0,0)),\n"
"                radial-gradient(1px 1px at 40px 70px, #aaa, rgba(0,0,0,0)),\n"
"                radial-gradient(1px 1px at 90px 40px, #fff, rgba(0,0,0,0));\n"
"            background-repeat: repeat; background-size: 200px 200px;\n"
"            animation: twinkle 4s infinite alternate;\n"
"        }\n"
"        @keyframes twinkle { 0% {opacity: 0.5;} 100% {opacity: 1;} }\n"
"        .x-wing {\n"
"            position: absolute; width: 60px; height: 60px; z-index: -1;\n"
"            background: url('data:image/svg+xml;utf8,<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 100 100\"><polygon points=\"20,40 40,40 60,20 60,80 40,60 20,60\" fill=\"silver\"/><rect x=\"40\" y=\"45\" width=\"40\" height=\"10\" fill=\"gray\"/></svg>') no-repeat center;\n"
"            background-size: contain; animation: fly-xwing 15s linear infinite; opacity: 0.7;\n"
"        }\n"
"        @keyframes fly-xwing {\n"
"            0% { top: 30%; left: -100px; transform: scale(0.5) rotate(15deg); }\n"
"            100% { top: 10%; left: 110vw; transform: scale(1.5) rotate(15deg); }\n"
"        }\n"
"        .tie-fighter {\n"
"            position: absolute; width: 50px; height: 50px; z-index: -1;\n"
"            background: url('data:image/svg+xml;utf8,<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 100 100\"><rect x=\"20\" y=\"10\" width=\"10\" height=\"80\" fill=\"silver\"/><rect x=\"70\" y=\"10\" width=\"10\" height=\"80\" fill=\"silver\"/><circle cx=\"50\" cy=\"50\" r=\"15\" fill=\"gray\"/><rect x=\"30\" y=\"45\" width=\"40\" height=\"10\" fill=\"gray\"/></svg>') no-repeat center;\n"
"            background-size: contain; animation: fly-tie 20s linear infinite; animation-delay: 5s; opacity: 0.5;\n"
"        }\n"
"        @keyframes fly-tie {\n"
"            0% { top: 80%; left: 110vw; transform: scale(1) rotate(-10deg); }\n"
"            100% { top: 50%; left: -100px; transform: scale(0.4) rotate(-10deg); }\n"
"        }\n"
"        .crawl-wrapper {\n"
"            transform: rotateX(15deg);\n"
"            transform-origin: center bottom;\n"
"            width: 100%; display: flex; flex-direction: column; align-items: center;\n"
"            margin-top: 30px;\n"
"            transition: transform 0.5s ease;\n"
"        }\n"
"        header {\n"
"            text-align: center; margin-bottom: 20px;\n"
"        }\n"
"        h1 {\n"
"            font-family: 'Orbitron', sans-serif; font-size: 3.2rem; margin: 0;\n"
"            text-transform: uppercase; letter-spacing: 8px;\n"
"            color: var(--primary); text-shadow: 0 0 15px rgba(255,232,31,0.65);\n"
"        }\n"
"        .subtitle {\n"
"            font-size: 1rem; color: #ccb918; margin-top: 8px; letter-spacing: 3px; text-transform: uppercase;\n"
"        }\n"
"        .container {\n"
"            display: flex; flex-direction: column; gap: 20px; width: 95%; max-width: 1000px;\n"
"        }\n"
"        .speech-synth-card {\n"
"            align-self: center; width: 100%; max-width: 900px;\n"
"            box-shadow: 0 0 30px rgba(255, 232, 31, 0.2), inset 0 0 15px rgba(255, 232, 31, 0.05);\n"
"            border: 2px solid var(--primary) !important;\n"
"            backdrop-filter: blur(10px); background: rgba(10, 10, 10, 0.9);\n"
"            padding: 25px !important;\n"
"            border-radius: 10px;\n"
"        }\n"
"        .speech-synth-card:hover {\n"
"            box-shadow: 0 0 40px rgba(255, 232, 31, 0.4);\n"
"        }\n"
"        .speech-synth-card h2 {\n"
"            font-size: 1.6rem !important;\n"
"            color: var(--primary) !important;\n"
"            border-bottom: 2px solid rgba(255, 232, 31, 0.3) !important;\n"
"            padding-bottom: 8px !important;\n"
"            margin-bottom: 15px !important;\n"
"        }\n"
"        .speech-synth-card input[type=\"text\"] {\n"
"            border-color: var(--primary) !important;\n"
"            color: var(--primary) !important;\n"
"            font-size: 1rem !important;\n"
"            padding: 11px !important;\n"
"            border-radius: 4px;\n"
"        }\n"
"        .speech-synth-card input[type=\"text\"]:focus {\n"
"            box-shadow: 0 0 15px rgba(255, 232, 31, 0.5) !important;\n"
"        }\n"
"        .speech-synth-card .btn {\n"
"            border-color: var(--primary) !important;\n"
"            color: var(--primary) !important;\n"
"            font-size: 1rem !important;\n"
"            padding: 11px 22px !important;\n"
"            border-radius: 4px;\n"
"        }\n"
"        .speech-synth-card .btn:hover {\n"
"            background: var(--primary) !important;\n"
"            color: var(--bg) !important;\n"
"            box-shadow: 0 0 20px rgba(255, 232, 31, 0.6) !important;\n"
"        }\n"
"        .divider {\n"
"            height: 2px; width: 70%;\n"
"            background: linear-gradient(90deg, transparent, rgba(255, 232, 31, 0.4), transparent); margin: 10px auto;\n"
"            box-shadow: 0 0 8px rgba(255, 232, 31, 0.3);\n"
"        }\n"
"        .lower-section {\n"
"            display: grid; grid-template-columns: repeat(3, 1fr); gap: 20px; width: 100%;\n"
"        }\n"
"        @media (max-width: 950px) {\n"
"            .lower-section {\n"
"                grid-template-columns: 1fr;\n"
"            }\n"
"            .divider {\n"
"                width: 80%;\n"
"            }\n"
"        }\n"
"        .card {\n"
"            background: var(--panel); border: 2px solid var(--primary);\n"
"            border-radius: 8px; padding: 20px; position: relative;\n"
"            box-shadow: 0 0 15px rgba(255,232,31,0.08), inset 0 0 12px rgba(255,232,31,0.04);\n"
"            transition: transform 0.3s ease, box-shadow 0.3s ease;\n"
"            cursor: url('data:image/svg+xml;utf8,<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"32\" height=\"32\" viewBox=\"0 0 32 32\"><rect x=\"14\" y=\"2\" width=\"4\" height=\"20\" fill=\"%23ff3333\" rx=\"2\"/><rect x=\"13\" y=\"22\" width=\"6\" height=\"8\" fill=\"silver\" rx=\"1\"/></svg>') 16 16, auto;\n"
"        }\n"
"        .card:hover {\n"
"            transform: translateY(-3px); box-shadow: 0 0 25px rgba(255,232,31,0.25);\n"
"        }\n"
"        h2 {\n"
"            font-family: 'Orbitron', sans-serif; margin-top: 0; font-size: 1.3rem; color: var(--primary);\n"
"            border-bottom: 2px solid rgba(255,232,31,0.25); padding-bottom: 8px; margin-bottom: 15px;\n"
"        }\n"
"        .status-row {\n"
"            display: flex; justify-content: space-between; margin-bottom: 12px; align-items: center;\n"
"        }\n"
"        .label { color: #ccb918; font-size: 0.85rem; text-transform: uppercase; letter-spacing: 1.2px; }\n"
"        .value { font-weight: 600; color: #fff; font-family: 'Orbitron', sans-serif; font-size: 0.9rem; }\n"
"        .badge {\n"
"            background: rgba(255,232,31,0.1); color: var(--primary); padding: 4px 10px;\n"
"            border-radius: 4px; font-size: 0.75rem; font-weight: bold; border: 1px solid var(--primary);\n"
"        }\n"
"        .btn {\n"
"            background: rgba(0,0,0,0.5); border: 2px solid var(--primary); color: var(--primary);\n"
"            padding: 10px 18px; font-family: 'Orbitron', sans-serif;\n"
"            font-weight: bold; letter-spacing: 1.5px; width: 100%; transition: all 0.3s ease;\n"
"            text-transform: uppercase; margin-top: 10px;\n"
"            cursor: url('data:image/svg+xml;utf8,<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"32\" height=\"32\" viewBox=\"0 0 32 32\"><rect x=\"14\" y=\"2\" width=\"4\" height=\"20\" fill=\"%23ff3333\" rx=\"2\"/><rect x=\"13\" y=\"22\" width=\"6\" height=\"8\" fill=\"silver\" rx=\"1\"/></svg>') 16 16, pointer;\n"
"        }\n"
"        .btn:hover {\n"
"            background: var(--primary); color: var(--bg); box-shadow: 0 0 15px rgba(255,232,31,0.5);\n"
"        }\n"
"        .input-group {\n"
"            display: flex; gap: 12px; margin-top: 15px; align-items: stretch; width: 100%;\n"
"        }\n"
"        .input-group input[type=\"text\"] {\n"
"            flex-grow: 1; margin-bottom: 0 !important;\n"
"        }\n"
"        .input-group .btn {\n"
"            width: auto !important; margin-top: 0 !important; white-space: nowrap;\n"
"        }\n"
"        @media (max-width: 768px) {\n"
"            .input-group {\n"
"                flex-direction: column;\n"
"            }\n"
"            .input-group .btn {\n"
"                width: 100% !important;\n"
"            }\n"
"        }\n"
"        input[type=\"text\"] {\n"
"            width: 100%; padding: 10px; background: rgba(0,0,0,0.7); border: 1px solid var(--primary);\n"
"            color: var(--primary); font-family: 'Orbitron', sans-serif; margin-bottom: 10px;\n"
"            box-sizing: border-box; transition: all 0.3s ease; text-transform: uppercase;\n"
"        }\n"
"        input[type=\"text\"]:focus {\n"
"            outline: none; box-shadow: 0 0 20px rgba(255,232,31,0.5);\n"
"        }\n"
"        input[type=\"text\"]::placeholder { color: rgba(255,232,31,0.5); }\n"
"        .coming-soon {\n"
"            position: absolute; top: 20px; right: 20px; font-size: 0.8rem; color: var(--accent);\n"
"            border: 1.5px solid var(--accent); padding: 5px 12px; font-family: 'Orbitron', sans-serif;\n"
"            border-radius: 4px; box-shadow: 0 0 10px rgba(255, 51, 51, 0.4);\n"
"        }\n"
"    </style>\n"
"</head>\n"
"<body>\n"
"    <div class=\"stars\"></div>\n"
"    <div class=\"x-wing\"></div>\n"
"    <div class=\"tie-fighter\"></div>\n"
"    <div class=\"crawl-wrapper\">\n"
"        <header>\n"
"            <h1>R2-D2</h1>\n"
"            <div class=\"subtitle\">Astromech Diagnostics & Control Panel</div>\n"
"            <div id=\"galactic-clock\" style=\"font-family: 'Orbitron', sans-serif; font-size: 1.1rem; color: var(--primary); text-shadow: 0 0 10px rgba(255,232,31,0.65); margin-top: 10px; letter-spacing: 3px; text-transform: uppercase;\">GST: --:--:--</div>\n"
"        </header>\n"
"        <div class=\"container\">\n"
"            <!-- Featured Centred Card -->\n"
"            <div class=\"card speech-synth-card\">\n"
"                <h2>Speech Synthesizer</h2>\n"
"                <p style=\"color: var(--primary); font-size: 0.9rem; margin-bottom: 15px; line-height: 1.5; opacity: 0.85;\">\n"
"                    AWAITING TEXT INPUT. DROID WILL PARSE CHARACTERS AND GENERATE PROCEDURAL TONES.\n"
"                </p>\n"
"                <div class=\"input-group\">\n"
"                    <input type=\"text\" id=\"speak-input\" placeholder=\"ENTER PHRASE TO TRANSLATE...\">\n"
"                    <button id=\"speak-btn\" class=\"btn\">Transmit to Droid</button>\n"
"                </div>\n"
"            </div>\n"
"            \n"
"            <!-- Glowing Separator Divider -->\n"
"            <div class=\"divider\"></div>\n"
"            \n"
"            <!-- Lower Section Stacked Grid -->\n"
"            \n"
"            <div class=\"lower-section\">\n"
"                <div class=\"card\">\n"
"                    <h2>System Status</h2>\n"
"                    <div class=\"status-row\">\n"
"                        <span class=\"label\">Main Processor</span>\n"
"                        <span class=\"value\">ESP32-P4</span>\n"
"                    </div>\n"
"                    <div class=\"status-row\">\n"
"                        <span class=\"label\">Network Co-processor</span>\n"
"                        <span class=\"value\">ESP32-C6</span>\n"
"                    </div>\n"
"                    <div class=\"status-row\">\n"
"                        <span class=\"label\">Current Mode</span>\n"
"                        <span id=\"current-screen-mode\" class=\"badge\">AUDIO / IDLE</span>\n"
"                    </div>\n"
"                    <div class=\"status-row\">\n"
"                        <span class=\"label\">Audio Synth</span>\n"
"                        <span class=\"value\" style=\"color: var(--primary); text-shadow: 0 0 6px rgba(255, 232, 31, 0.5);\">ONLINE</span>\n"
"                    </div>\n"
"                    <div class=\"status-row\">\n"
"                        <span class=\"label\">System Time</span>\n"
"                        <span id=\"system-time-val\" class=\"value\" style=\"color: var(--primary); text-shadow: 0 0 6px rgba(255, 232, 31, 0.5);\">SYNCING...</span>\n"
"                    </div>\n"
"                </div>\n"
"                \n"
"                <div class=\"card\" style=\"grid-column: span 2;\">\n"
"                    <h2>Display Controller</h2>\n"
"                    <div style=\"display: flex; gap: 15px; margin-bottom: 15px; align-items: center; justify-content: space-between;\">\n"
"                        <div style=\"display: flex; flex-direction: column; gap: 4px;\">\n"
"                            <span class=\"label\">Physical Screen</span>\n"
"                            <span id=\"screen-status-badge\" class=\"badge\" style=\"width: fit-content;\">INACTIVE</span>\n"
"                        </div>\n"
"                        <button id=\"toggle-mode-btn\" class=\"btn\" style=\"width: auto; margin: 0; padding: 8px 16px; font-size: 0.85rem;\">Toggle Screen Mode</button>\n"
"                    </div>\n"
"                    \n"
"                    <p style=\"color: #ccb918; font-size: 0.85rem; margin-bottom: 12px; line-height: 1.5;\">\n"
"                        UPLOAD NEW GIF OR SELECT FROM PALETTE BELOW TO PROJECT ONTO LCD.\n"
"                    </p>\n"
"                    <div class=\"input-group\" style=\"margin-top: 0;\">\n"
"                        <input type=\"file\" id=\"gif-upload\" accept=\"image/gif\" style=\"flex-grow:1; color: var(--primary); font-family: Orbitron, sans-serif; font-size: 0.85rem;\">\n"
"                        <button id=\"gif-btn\" class=\"btn\" style=\"padding: 8px 16px; font-size: 0.85rem; width: auto !important; margin-top: 0 !important;\" disabled>Upload GIF</button>\n"
"                    </div>\n"
"                    <div id=\"gif-preview\" style=\"margin-top:12px; text-align:center;\"></div>\n"
"                    <div id=\"gif-list-container\"></div>\n"
"                </div>\n"
"            </div>\n"
"\n"
"        </div>\n"
"    </div>\n"
"            </div>\n"
"        </div>\n"
"    </div>\n"
"    <script>\n"
"        document.getElementById('speak-btn').addEventListener('click', () => {\n"
"            const input = document.getElementById('speak-input');\n"
"            const btn = document.getElementById('speak-btn');\n"
"            const text = input.value.trim();\n"
"            if (!text) return;\n"
"            \n"
"            btn.disabled = true;\n"
"            btn.innerText = 'TRANSMITTING...';\n"
"            btn.style.opacity = '0.5';\n"
"            \n"
"            fetch('/api/speak', {\n"
"                method: 'POST',\n"
"                headers: { 'Content-Type': 'text/plain' },\n"
"                body: text\n"
"            }).then(res => {\n"
"                if (res.ok) {\n"
"                    input.value = '';\n"
"                }\n"
"            }).catch(err => {\n"
"                console.error('Error sending speak request:', err);\n"
"            }).finally(() => {\n"
"                btn.disabled = false;\n"
"                btn.innerText = 'Transmit to Droid';\n"
"                btn.style.opacity = '1';\n"
"            });\n"
"        });\n"
"        document.getElementById('speak-input').addEventListener('keydown', (e) => {\n"
"            if (e.key === 'Enter') {\n"
"                document.getElementById('speak-btn').click();\n"
"            }\n"
"        });\n"
"\n"
"        // Dynamic Display updates & controls\n"
"        const updateStatus = () => {\n"
"            fetch('/api/mode')\n"
"                .then(res => res.text())\n"
"                .then(mode => {\n"
"                    document.getElementById('current-screen-mode').innerText = mode === 'DISPLAY' ? 'DISPLAY MODE' : 'AUDIO / IDLE';\n"
"                    const badge = document.getElementById('screen-status-badge');\n"
"                    if (mode === 'DISPLAY') {\n"
"                        badge.innerText = 'ACTIVE';\n"
"                        badge.style.borderColor = 'var(--primary)';\n"
"                        badge.style.color = 'var(--primary)';\n"
"                        badge.style.boxShadow = '0 0 10px rgba(255, 232, 31, 0.4)';\n"
"                    } else {\n"
"                        badge.innerText = 'INACTIVE';\n"
"                        badge.style.borderColor = 'var(--accent)';\n"
"                        badge.style.color = 'var(--accent)';\n"
"                        badge.style.boxShadow = '0 0 10px rgba(255, 51, 51, 0.4)';\n"
"                    }\n"
"                }).catch(err => console.error('Error fetching mode:', err));\n"
"\n"
"            fetch('/api/quote')\n"
"                .then(res => res.text())\n"
"                .then(quote => {\n"
"                    document.getElementById('current-quote').innerText = quote;\n"
"                }).catch(err => console.error('Error fetching quote:', err));\n"
"\n"
"            fetch('/api/time')\n"
"                .then(res => res.text())\n"
"                .then(time => {\n"
"                    document.getElementById('galactic-clock').innerText = 'GST: ' + time;\n"
"                    document.getElementById('system-time-val').innerText = time;\n"
"                }).catch(err => console.error('Error fetching time:', err));\n"
"        };\n"
"\n"
"        // Poll status every 3 seconds\n"
"        setInterval(updateStatus, 3000);\n"
"        updateStatus();\n"
"\n"
"        // Toggle Screen Mode click handler\n"
"        document.getElementById('toggle-mode-btn').addEventListener('click', () => {\n"
"            const btn = document.getElementById('toggle-mode-btn');\n"
"            btn.disabled = true;\n"
"            btn.innerText = 'TRANSMITTING...';\n"
"            fetch('/api/mode', { method: 'POST' })\n"
"                .then(res => res.text())\n"
"                .then(mode => {\n"
"                    updateStatus();\n"
"                })\n"
"                .catch(err => {\n"
"                    console.error('Error toggling mode:', err);\n"
"                })\n"
"                .finally(() => {\n"
"                    btn.disabled = false;\n"
"                    btn.innerText = 'Toggle Screen Mode';\n"
"                });\n"
"        });\n"
"\n"
"        // GIF Upload handler\n"
"        const gifUpload = document.getElementById('gif-upload');\n"
"        const gifBtn = document.getElementById('gif-btn');\n"
"        const gifPreview = document.getElementById('gif-preview');\n"
"        const gifListDiv = document.createElement('div');\n"
"        gifListDiv.id = 'gif-list';\n"
"        gifListDiv.style.marginTop = '20px';\n"
"        gifListDiv.style.display = 'flex';\n"
"        gifListDiv.style.flexWrap = 'wrap';\n"
"        gifListDiv.style.justifyContent = 'center';\n"
"        // Append it below the gif-preview div, inside the card\n"
"        document.getElementById('gif-list-container').appendChild(gifListDiv);\n"
"\n"
"        const loadGifList = () => {\n"
"            fetch('/api/gifs')\n"
"                .then(res => {\n"
"                    if (!res.ok) throw new Error('SD Card Error');\n"
"                    return res.json();\n"
"                })\n"
"                .then(list => {\n"
"                    gifListDiv.innerHTML = '';\n"
"                    list.filter(name => !name.startsWith('_')).forEach(name => {\n"
"                        const wrapper = document.createElement('div');\n"
"                        wrapper.style.display = 'inline-flex';\n"
"                        wrapper.style.flexDirection = 'column';\n"
"                        wrapper.style.alignItems = 'center';\n"
"                        wrapper.style.margin = '8px';\n"
"                        wrapper.style.gap = '6px';\n"
"                        const thumb = document.createElement('img');\n"
"                        thumb.src = `/api/gif?name=${name}`;\n"
"                        thumb.title = name;\n"
"                        thumb.style.maxWidth = '80px';\n"
"                        thumb.style.borderRadius = '6px';\n"
"                        thumb.style.border = '1px solid var(--primary)';\n"
"                        thumb.style.cursor = 'pointer';\n"
"                        thumb.addEventListener('click', () => {\n"
"                            gifPreview.innerHTML = `<img src=\"/api/gif?name=${name}\" style=\"max-width:180px;max-height:180px;object-fit:contain;border:1px solid var(--primary);border-radius:6px;\"/>`;\n"
"                        });\n"
"                        wrapper.appendChild(thumb);\n"
""
"                        const playBtn = document.createElement('button');\n"
"                        playBtn.innerHTML = '&#9654; PLAY';\n"
"                        playBtn.style.background = 'rgba(0,0,0,0.7)';\n"
"                        playBtn.style.border = '1.5px solid var(--primary)';\n"
"                        playBtn.style.color = 'var(--primary)';\n"
"                        playBtn.style.padding = '4px 12px';\n"
"                        playBtn.style.fontFamily = 'Orbitron, sans-serif';\n"
"                        playBtn.style.fontSize = '0.7rem';\n"
"                        playBtn.style.cursor = 'pointer';\n"
"                        playBtn.style.borderRadius = '4px';\n"
"                        playBtn.style.letterSpacing = '1px';\n"
"                        playBtn.style.transition = 'all 0.3s ease';\n"
"                        playBtn.addEventListener('mouseenter', () => {\n"
"                            playBtn.style.background = 'var(--primary)';\n"
"                            playBtn.style.color = 'var(--bg)';\n"
"                            playBtn.style.boxShadow = '0 0 15px rgba(255,232,31,0.6)';\n"
"                        });\n"
"                        playBtn.addEventListener('mouseleave', () => {\n"
"                            playBtn.style.background = 'rgba(0,0,0,0.7)';\n"
"                            playBtn.style.color = 'var(--primary)';\n"
"                            playBtn.style.boxShadow = 'none';\n"
"                        });\n"
"                        playBtn.addEventListener('click', () => {\n"
"                            playBtn.innerHTML = '&#9654; PLAYING...';\n"
"                            playBtn.disabled = true;\n"
"                            fetch(`/api/gif/play?name=${encodeURIComponent(name)}`)\n"
"                                .then(r => { if (r.ok) { updateStatus(); } })\n"
"                                .catch(err => console.error('Play GIF error:', err))\n"
"                                .finally(() => { playBtn.innerHTML = '&#9654; PLAY'; playBtn.disabled = false; });\n"
"                        });\n"
"                        wrapper.appendChild(playBtn);\n"
"                        gifListDiv.appendChild(wrapper);\n"
"                    });\n"
"                })\n"
"                .catch(err => {\n"
"                    gifListDiv.innerHTML = `<div style=\"background: rgba(255,0,0,0.2); border: 1px solid red; color: #ff5555; padding: 15px; border-radius: 8px; text-align: center; max-width: 400px; font-family: 'Courier New', monospace;\">\n"
"                        <b>[!] SD CARD MOUNT FAILED</b><br><br>\n"
"                        The ESP32 cannot read the physical SD card. Please ensure the SD card is properly inserted into the ESP32 and contains the <code>gifs</code> folder.\n"
"                    </div>`;\n"
"                });\n"
"        };\n"
"\n"
"        gifUpload.addEventListener('change', () => {\n"
"            gifBtn.disabled = !gifUpload.files.length;\n"
"        });\n"
"        gifBtn.addEventListener('click', () => {\n"
"            const file = gifUpload.files[0];\n"
"            if (!file) return;\n"
"            gifBtn.disabled = true;\n"
"            gifBtn.innerText = 'UPLOADING...';\n"
"            const reader = new FileReader();\n"
"            reader.onload = () => {\n"
"                fetch('/api/gif?name=' + encodeURIComponent(file.name), {\n"
"                    method: 'POST',\n"
"                    headers: { 'Content-Type': 'application/octet-stream' },\n"
"                    body: reader.result\n"
"                }).then(res => {\n"
"                    if (res.ok) {\n"
"                        loadGifList();\n"
"                        gifPreview.innerHTML = `<img src=\"${URL.createObjectURL(file)}\" style=\"max-width:180px;max-height:180px;object-fit:contain;border:1px solid var(--primary);border-radius:6px;\"/>`;\n"
"                    }\n"
"                }).catch(err => console.error('GIF upload error:', err))\n"
"                .finally(() => {\n"
"                    gifBtn.disabled = false;\n"
"                    gifBtn.innerText = 'Upload GIF';\n"
"                });\n"
"            };\n"
"            reader.readAsArrayBuffer(file);\n"
"        });\n"
"        loadGifList();\n"
"    </script>\n"
"</body>\n"
"</html>\n"
;

/* HTTP GET handler for the root path */
static esp_err_t get_handler(httpd_req_t *req)
{
    // Set content type
    httpd_resp_set_type(req, "text/html");
    
    // Send the dashboard HTML string
    httpd_resp_send(req, dashboard_html, HTTPD_RESP_USE_STRLEN);
    
    return ESP_OK;
}

/* HTTP POST handler for custom R2-D2 speak request */
static esp_err_t speak_post_handler(httpd_req_t *req)
{
    char buf[256];
    int ret, remaining = req->content_len;

    if (remaining <= 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Content-Length required");
        return ESP_FAIL;
    }

    if (remaining >= sizeof(buf)) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Phrase too long");
        return ESP_FAIL;
    }

    int cur = 0;
    while (remaining > 0) {
        if ((ret = httpd_req_recv(req, buf + cur, remaining)) <= 0) {
            if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
                continue;
            }
            return ESP_FAIL;
        }
        cur += ret;
        remaining -= ret;
    }
    buf[cur] = '\0';

    ESP_LOGI(TAG, "Speech request received: '%s'", buf);

    // Synthesize the text deterministically
    r2d2_speak_text(buf);

    httpd_resp_sendstr(req, "OK");
    return ESP_OK;
}

/* HTTP GET handler for fetching current system time */
static esp_err_t time_get_handler(httpd_req_t *req)
{
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);
    
    char time_str[64];
    if (timeinfo.tm_year < 75) {
        uint32_t sec = xTaskGetTickCount() / configTICK_RATE_HZ;
        snprintf(time_str, sizeof(time_str), "%02lu:%02lu:%02lu (UPTIME)", 
                 (sec / 3600) % 24, (sec / 60) % 60, sec % 60);
    } else {
        char formatted_time[32];
        strftime(formatted_time, sizeof(formatted_time), "%H:%M:%S", &timeinfo);
        snprintf(time_str, sizeof(time_str), "%s (IST)", formatted_time);
    }
    
    httpd_resp_set_type(req, "text/plain");
    httpd_resp_sendstr(req, time_str);
    return ESP_OK;
}

/* HTTP GET handler for mode request */
static esp_err_t mode_get_handler(httpd_req_t *req)
{
    r2d2_mode_t mode = r2d2_get_mode();
    const char *mode_str = (mode == MODE_DISPLAY) ? "DISPLAY" : "AUDIO_IDLE";
    httpd_resp_set_type(req, "text/plain");
    httpd_resp_sendstr(req, mode_str);
    return ESP_OK;
}

/* HTTP POST handler for setting/toggling mode request */
static esp_err_t mode_post_handler(httpd_req_t *req)
{
    char buf[32];
    int ret, remaining = req->content_len;

    if (remaining > 0) {
        if (remaining >= sizeof(buf)) {
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Payload too long");
            return ESP_FAIL;
        }
        int cur = 0;
        while (remaining > 0) {
            ret = httpd_req_recv(req, buf + cur, remaining);
            if (ret <= 0) {
                if (ret == HTTPD_SOCK_ERR_TIMEOUT) continue;
                return ESP_FAIL;
            }
            cur += ret;
            remaining -= ret;
        }
        buf[cur] = '\0';

        if (strcmp(buf, "DISPLAY") == 0) {
            r2d2_set_mode(MODE_DISPLAY);
        } else if (strcmp(buf, "AUDIO_IDLE") == 0) {
            r2d2_set_mode(MODE_AUDIO_IDLE);
        } else {
            r2d2_mode_t curr = r2d2_get_mode();
            r2d2_set_mode(curr == MODE_DISPLAY ? MODE_AUDIO_IDLE : MODE_DISPLAY);
        }
    } else {
        r2d2_mode_t curr = r2d2_get_mode();
        r2d2_set_mode(curr == MODE_DISPLAY ? MODE_AUDIO_IDLE : MODE_DISPLAY);
    }

    // refresh if now DISPLAY
    r2d2_mode_t mode = r2d2_get_mode();
    if (mode == MODE_DISPLAY) {
        r2d2_display_refresh_quote();
    }
    const char *mode_str = (mode == MODE_DISPLAY) ? "DISPLAY" : "AUDIO_IDLE";
    httpd_resp_set_type(req, "text/plain");
    httpd_resp_sendstr(req, mode_str);
    return ESP_OK;
}

/* HTTP GET handler for fetching active quote */
static esp_err_t quote_get_handler(httpd_req_t *req)
{
    char buf[128];
    r2d2_display_get_quote(buf, sizeof(buf));
    httpd_resp_set_type(req, "text/plain");
    httpd_resp_sendstr(req, buf);
    return ESP_OK;
}

/* HTTP POST handler for projecting a custom quote */
static esp_err_t quote_post_handler(httpd_req_t *req)
{
    char buf[128];
    int ret, remaining = req->content_len;

    if (remaining <= 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Content-Length required");
        return ESP_FAIL;
    }

    if (remaining >= sizeof(buf)) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Quote too long");
        return ESP_FAIL;
    }

    int cur = 0;
    while (remaining > 0) {
        if ((ret = httpd_req_recv(req, buf + cur, remaining)) <= 0) {
            if (ret == HTTPD_SOCK_ERR_TIMEOUT) continue;
            return ESP_FAIL;
        }
        cur += ret;
        remaining -= ret;
    }
    buf[cur] = '\0';

    ESP_LOGI(TAG, "Quote projection request: '%s'", buf);
    r2d2_display_set_quote(buf);

    httpd_resp_sendstr(req, "OK");
    return ESP_OK;
}

static esp_err_t gif_get_handler(httpd_req_t *req);
static esp_err_t gif_list_handler(httpd_req_t *req);
static esp_err_t gif_get_handler(httpd_req_t *req)
{
    // Check for optional filename query parameter
    char query[64];
    char filename[64] = "latest.gif"; // default
    if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK) {
        // Parse name=\"xxx\"
        const char *key = "name=";
        char *p = strstr(query, key);
        if (p) {
            p += strlen(key);
            // copy up to '&' or end
            size_t i = 0;
            while (*p && *p != '&' && i < sizeof(filename) - 1) {
                filename[i++] = *p++;
            }
            filename[i] = '\0';
        }
    }
    char file_path[128];
    snprintf(file_path, sizeof(file_path), "/sdcard/gifs/%s", filename);
    FILE *f = fopen(file_path, "rb");
    if (!f) {
        // Fallback to gif
        snprintf(file_path, sizeof(file_path), "/sdcard/gif/%s", filename);
        f = fopen(file_path, "rb");
        if (!f) {
            httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "GIF not found");
            return ESP_FAIL;
        }
    }

    httpd_resp_set_type(req, "image/gif");
    char *buf = malloc(4096);
    if (!buf) {
        fclose(f);
        return ESP_FAIL;
    }
    size_t chunksize;
    while ((chunksize = fread(buf, 1, 4096, f)) > 0) {
        if (httpd_resp_send_chunk(req, buf, chunksize) != ESP_OK) {
            fclose(f);
            free(buf);
            ESP_LOGE(TAG, "File sending failed!");
            return ESP_FAIL;
        }
    }
    fclose(f);
    free(buf);
    // Send empty chunk to signal completion
    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}

static esp_err_t gif_list_handler(httpd_req_t *req)
{
    const char *dir_path = "/sdcard/gifs";
    DIR *d = opendir(dir_path);
    if (!d) {
        // Fallback to "gif"
        dir_path = "/sdcard/gif";
        d = opendir(dir_path);
    }
    
    if (!d) {
        // Check if SD card is even mounted by checking root
        DIR *root = opendir("/sdcard");
        if (root) {
            // SD card is mounted, but neither folder exists. Create 'gifs' and return empty array.
            closedir(root);
            mkdir("/sdcard/gifs", 0777);
            httpd_resp_set_type(req, "application/json");
            httpd_resp_sendstr(req, "[]");
            return ESP_OK;
        } else {
            // SD card is physically missing or failed to mount
            httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "SD Card not mounted");
            return ESP_FAIL;
        }
    }
    
    struct dirent *entry;
    // Build JSON array of filenames
    char *json = malloc(1024);
    if (!json) {
        closedir(d);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Memory error");
        return ESP_FAIL;
    }
    strcpy(json, "[");
    bool first = true;
    while ((entry = readdir(d)) != NULL) {
        if (entry->d_type == DT_REG) {
            // Ignore hidden and macOS metadata files starting with '.' or '_'
            if (entry->d_name[0] == '.' || entry->d_name[0] == '_') continue;
            // Only include files ending in .gif
            char *ext = strrchr(entry->d_name, '.');
            if (ext && strcasecmp(ext, ".gif") == 0) {
                // Validate it's a real GIF by checking magic header
                char fpath[280];
                snprintf(fpath, sizeof(fpath), "%s/%s", dir_path, entry->d_name);
                FILE *fp = fopen(fpath, "rb");
                if (fp) {
                    char hdr[4] = {0};
                    size_t nr = fread(hdr, 1, 4, fp);
                    fclose(fp);
                    if (nr < 4 || memcmp(hdr, "GIF8", 4) != 0) {
                        continue; // Not a real GIF file, skip it
                    }
                }
                if (!first) strcat(json, ",");
                first = false;
                strcat(json, "\"");
                strcat(json, entry->d_name);
                strcat(json, "\"");
            }
        }
    }
    strcat(json, "]");
    closedir(d);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json, strlen(json));
    free(json);
    return ESP_OK;
}

static void urldecode(char *dst, const char *src);

static esp_err_t gif_post_handler(httpd_req_t *req)
{
    int remaining = req->content_len;
    if (remaining <= 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "No content");
        return ESP_FAIL;
    }

    // Ensure SD card directory exists
    const char *dir_path = "/sdcard/gifs";
    struct stat st = {0};
    if (stat(dir_path, &st) == -1) {
        mkdir(dir_path, 0777);
    }

    // Use original filename from query param, fallback to "upload.gif"
    char query[128];
    char upload_name[64] = "upload.gif";
    if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK) {
        const char *key = "name=";
        char *p = strstr(query, key);
        if (p) {
            p += strlen(key);
            char encoded_name[64];
            size_t i = 0;
            while (*p && *p != '&' && i < sizeof(encoded_name) - 1) {
                encoded_name[i++] = *p++;
            }
            encoded_name[i] = '\0';
            urldecode(upload_name, encoded_name);
        }
    }
    char file_path[128];
    snprintf(file_path, sizeof(file_path), "/sdcard/gifs/%s", upload_name);

    FILE *f = fopen(file_path, "wb");
    if (!f) {
        ESP_LOGE(TAG, "Failed to open %s for writing", file_path);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "File write error");
        return ESP_FAIL;
    }

    // Stream the data in 4KB chunks directly to file
    char *buf = malloc(4096);
    if (!buf) {
        fclose(f);
        return ESP_FAIL;
    }
    int total_received = 0;
    while (remaining > 0) {
        int to_recv = remaining < 4096 ? remaining : 4096;
        int ret = httpd_req_recv(req, buf, to_recv);
        if (ret <= 0) {
            if (ret == HTTPD_SOCK_ERR_TIMEOUT) continue;
            fclose(f);
            free(buf);
            unlink(file_path); // delete incomplete file
            ESP_LOGE(TAG, "Upload failed during recv");
            return ESP_FAIL;
        }
        fwrite(buf, 1, ret, f);
        total_received += ret;
        remaining -= ret;
    }
    fclose(f);
    free(buf);

    ESP_LOGI(TAG, "GIF upload saved (%d bytes) to %s", total_received, file_path);
    httpd_resp_sendstr(req, "OK");
    return ESP_OK;
}

// Utility function for URL decoding
static void urldecode(char *dst, const char *src)
{
    char a, b;
    while (*src) {
        if ((*src == '%') &&
            ((a = src[1]) && (b = src[2])) &&
            (isxdigit((unsigned char)a) && isxdigit((unsigned char)b))) {
            if (a >= 'a') a -= 'a' - 'A';
            if (a >= 'A') a -= ('A' - 10);
            else a -= '0';
            if (b >= 'a') b -= 'a' - 'A';
            if (b >= 'A') b -= ('A' - 10);
            else b -= '0';
            *dst++ = 16 * a + b;
            src += 3;
        } else if (*src == '+') {
            *dst++ = ' ';
            src++;
        } else {
            *dst++ = *src++;
        }
    }
    *dst = '\0';
}

/* HTTP GET handler for triggering a GIF playback on physical screen */
static esp_err_t gif_play_handler(httpd_req_t *req)
{
    char query[128];
    char filename[64] = "upload.gif";
    if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK) {
        const char *key = "name=";
        char *p = strstr(query, key);
        if (p) {
            p += strlen(key);
            char encoded_name[64];
            size_t i = 0;
            while (*p && *p != '&' && i < sizeof(encoded_name) - 1) {
                encoded_name[i++] = *p++;
            }
            encoded_name[i] = '\0';
            urldecode(filename, encoded_name);
        }
    }

    // Verify file exists on SD card
    char file_path[128];
    snprintf(file_path, sizeof(file_path), "/sdcard/gifs/%s", filename);
    struct stat st;
    if (stat(file_path, &st) != 0) {
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "GIF not found on SD card");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Playing GIF on display: %s", filename);
    r2d2_display_play_gif(filename);

    httpd_resp_set_type(req, "text/plain");
    httpd_resp_sendstr(req, "OK");
    return ESP_OK;
}

void r2d2_webserver_start(void)
{
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_uri_handlers = 16;  // Default is 8, we need 11+

    // Start the httpd server
    ESP_LOGI(TAG, "Starting web server on port: '%d'", config.server_port);
    if (httpd_start(&server, &config) == ESP_OK) {
        // Set URI handlers
        ESP_LOGI(TAG, "Registering URI handlers");
        
        httpd_uri_t uri_get = {
            .uri      = "/",
            .method   = HTTP_GET,
            .handler  = get_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &uri_get);

        httpd_uri_t uri_speak = {
            .uri      = "/api/speak",
            .method   = HTTP_POST,
            .handler  = speak_post_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &uri_speak);

        httpd_uri_t uri_mode_get = {
            .uri      = "/api/mode",
            .method   = HTTP_GET,
            .handler  = mode_get_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &uri_mode_get);

        httpd_uri_t uri_mode_post = {
            .uri      = "/api/mode",
            .method   = HTTP_POST,
            .handler  = mode_post_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &uri_mode_post);

        httpd_uri_t uri_quote_get = {
            .uri      = "/api/quote",
            .method   = HTTP_GET,
            .handler  = quote_get_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &uri_quote_get);

        httpd_uri_t uri_quote_post = {
            .uri      = "/api/quote",
            .method   = HTTP_POST,
            .handler  = quote_post_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &uri_quote_post);


        // Existing handlers
        httpd_uri_t uri_gif_get = {
            .uri      = "/api/gif",
            .method   = HTTP_GET,
            .handler  = gif_get_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &uri_gif_get);
        httpd_uri_t uri_gif_list = {
            .uri      = "/api/gifs",
            .method   = HTTP_GET,
            .handler  = gif_list_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &uri_gif_list);
        httpd_uri_t uri_gif_post = {
            .uri      = "/api/gif",
            .method   = HTTP_POST,
            .handler  = gif_post_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &uri_gif_post);

        httpd_uri_t uri_gif_play = {
            .uri      = "/api/gif/play",
            .method   = HTTP_GET,
            .handler  = gif_play_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &uri_gif_play);

        httpd_uri_t uri_time_get = {
            .uri      = "/api/time",
            .method   = HTTP_GET,
            .handler  = time_get_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &uri_time_get);
    } else {
        ESP_LOGI(TAG, "Error starting server!");
    }
}
