# Bill of materials (v1 breadboard logger)

Rough prices in CAD, estimated from typical prices for these parts, not
checked against live listings. Treat them as "about this much", not quotes,
and check DigiKey, Amazon.ca or an Adafruit reseller before ordering.

| Part | Qty | Rough price (CAD) | Notes |
|---|---|---|---|
| ST NUCLEO-L432KC | 1 | $16-20 | Has a built-in ST-LINK: programming, debugging and USB serial over one cable |
| MPU-6050 breakout (GY-521) | 1 | $5-10 | Often sold in 3-packs for about the price of one. Clones exist; the firmware checks WHO_AM_I |
| microSD breakout, SPI | 1 | $5-12 | Adafruit #254 or similar. Must accept 3.3 V |
| microSD card, 8-32 GB | 1 | $8-12 | Format FAT32 |
| Tactile push button | 1 | $1 | Usually in a kit |
| Half-size breadboard | 1 | $5-8 | |
| Jumper wires (M-M, M-F) | 1 pack | $5-8 | |
| USB cable (micro-B) that carries data | 1 | $0-8 | The L432KC uses micro-USB; check it isn't charge-only |
| USB battery pack | 1 | $0-25 | For court tests. Many people already have one |
| Sweatband or wrist strap | 1 | $5-10 | To mount the IMU on the shooting wrist |
| 8-channel logic analyzer (Saleae clone) | 1 | $15-25 | Works with the free PulseView. For timing captures |

**Rough total: about $65-140**, depending on what's already in the parts bin.
The required parts alone (Nucleo, IMU, SD breakout, card, button, breadboard,
wires) are about $45-70.

v2 and v3 parts are listed in [roadmap.md](roadmap.md).
