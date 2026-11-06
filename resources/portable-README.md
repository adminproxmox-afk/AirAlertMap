# AirAlertMap @VERSION@ for Windows (portable Win32)

1. Extract the **entire ZIP** into a folder you can write to. Paths with spaces are supported.
2. Run **start.cmd**. You can also run AirAlertMap.exe directly.
3. Open Settings, select the service that issued your API key, enter it, and apply.

No application installer or administrator access is needed. Microsoft Edge
WebView2 Runtime must already be installed for the map. If it is missing,
start.cmd shows the official Microsoft download address; it never installs
anything automatically. Windows 10/11 with a current WebView2 Runtime is the
supported configuration. The application is 32-bit and also runs on 64-bit Windows.

Python 3.10 or newer is optional and is used for local history reports. The map,
live alerts, and settings work without Python. Python for Windows:
https://www.python.org/downloads/windows/

WebView2 Runtime: https://developer.microsoft.com/microsoft-edge/webview2/#download-section

The ZIP contains no API key, user preferences, observation history, diagnostics,
or browser profile. Your own data is created under
%LOCALAPPDATA%\AirAlertMap. Saved keys use the Windows protected credential
storage on your account; copying this portable folder does not transfer keys.
UkraineAlarm / Stfalcon and alerts.in.ua keys are not interchangeable.

Keep web, analytics, data, WebView2Loader.dll, and the accompanying license next
to AirAlertMap.exe. Do not start the application from inside a ZIP viewer.
Data attribution and boundary limitations are in data/NOTICE.md.
WebView2 loader license: WebView2-LICENSE.txt. The application icon is an
original geometric mark created for AirAlertMap.

This application supplements official warning channels. Local observation
history is not an official alert archive. Check official guidance during alerts.

To move the program, copy the extracted folder. To remove the program, delete
that folder. User data in LocalAppData and saved Windows credentials remain
separate; use the application's settings to remove a saved key.

release-manifest.json lists the version, size and SHA-256 of each shipped file.
The adjacent ZIP.sha256 file can be checked with Get-FileHash in PowerShell.
