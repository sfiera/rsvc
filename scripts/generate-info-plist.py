#!/usr/bin/env python

import sys

PLIST_TEMPLATE = """\
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>CFBundleDevelopmentRegion</key>
    <string>English</string>
    <key>CFBundleExecutable</key>
    <string>Rip Service</string>
    <key>CFBundleIconFile</key>
    <string>rip-service.icns</string>
    <key>CFBundleIdentifier</key>
    <string>org.rsvc.rip-service</string>
    <key>CFBundleInfoDictionaryVersion</key>
    <string>6.0</string>
    <key>CFBundleName</key>
    <string>Rip Service</string>
    <key>CFBundlePackageType</key>
    <string>APPL</string>
    <key>CFBundleSignature</key>
    <string>????</string>
    <key>CFBundleShortVersionString</key>
    <string>%(rip-service-version)s</string>
    <key>LSMinimumSystemVersion</key>
    <string>%(system-version)s</string>
    <key>CFBundleVersion</key>
    <string>1</string>
    <key>NSMainNibFile</key>
    <string>MainMenu</string>
    <key>NSPrincipalClass</key>
    <string>NSApplication</string>
</dict>
</plist>
"""

_, rip_service_version, system_version, out = sys.argv
with open(out, "w") as f:
    f.write(
        PLIST_TEMPLATE % {
            "rip-service-version": rip_service_version,
            "system-version": system_version,
        })
