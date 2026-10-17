#!/usr/bin/env python3
"""Genera esp32/SolarCleanerPro_ESP32/pagina_web.h a partir de www/index.html.

El ESP32 sirve esa página (comprimida con gzip) en http://192.168.4.1.
Ejecutar cada vez que se modifique www/index.html:

    python3 tools/generar_pagina.py
"""
import gzip, os, sys

raiz = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
origen = os.path.join(raiz, "www", "index.html")
destino = os.path.join(raiz, "esp32", "SolarCleanerPro_ESP32", "pagina_web.h")

with open(origen, "rb") as f:
    datos = gzip.compress(f.read(), compresslevel=9, mtime=0)

lineas = []
for i in range(0, len(datos), 16):
    lineas.append("  " + ", ".join("0x%02x" % b for b in datos[i:i + 16]))

with open(destino, "w", encoding="utf-8") as f:
    f.write("// Generado por tools/generar_pagina.py. No editar a mano.\n")
    f.write("#pragma once\n#include <Arduino.h>\n\n")
    f.write("const size_t PAGINA_GZ_LEN = %d;\n" % len(datos))
    f.write("const uint8_t PAGINA_GZ[] PROGMEM = {\n")
    f.write(",\n".join(lineas))
    f.write("\n};\n")

print("pagina_web.h generado: %d bytes comprimidos" % len(datos))
