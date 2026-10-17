# Verification record

Source: the local weather-station project completed on 15 October 2026.

The saved Wokwi sketch compiled and ran on the simulated ESP32. Startup checks printed `SELF-TEST PASS`. Readings of 28 C / 60%, 36 C / 83%, and 32 C / 74% exercised normal, alarm and recovery states. Wi-Fi connection, NTP synchronization and CA-validated HTTPS uploads succeeded. Feed readback identified 15 simulation entries; the high scenario returned HTTP 202 for the email request, and the original verification checked dispatch history and inbox receipt separately.

This is a record of that simulation, not a new test run or evidence of physical deployment. The original private dashboard, inbox screenshots, raw account data, configuration secrets, submission report and build caches are deliberately excluded from this public repository.

The optional physical ESP32 firmware has an earlier local Windows compiler-launch failure and no physical-board validation. Full 31-minute cooldown expiry, router interruption, outdoor accuracy and persistence between separate Wokwi sessions were not exercised.
