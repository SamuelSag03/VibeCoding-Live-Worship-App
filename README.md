# LiveKeys — App standalone de performance en vivo para tecladistas (estilo MainStage / Gig Performer)

App nativa para **Arch Linux** (y portable a Win/macOS) pensada para tecladistas de worship:
patches sin cortes de audio, hot-swap de teclados e interfaces, sync con DAWs y control
remoto por MIDI/OSC.

## ¿Por qué app standalone y no "DAW como motor"?

Descartamos usar Reaper/FL-clone como motor de audio porque en Linux un DAW GUI como engine
añade latencia, consumo y complejidad innecesaria (ventana, arrange view, etc.). En cambio usamos
**JUCE**, el mismo framework con el que están hechos muchos hosts profesionales: nos da engine de
audio de baja latencia (JACK/PipeWire), hosting de plugins **VST3 y LV2**, MIDI, OSC y UI en un
solo binario standalone. El DAW externo (Ableton Live, etc.) se sincroniza por **MIDI Clock / MMC**,
igual que MainStage.

## Arquitectura

```
src/
  Main.cpp               Punto de entrada, crea MessageManager + ventana principal
  core/
    AudioEngine          AudioDeviceManager (JACK primero), hot-swap de dispositivos,
                         callback de audio en tiempo real, medidor de latencia/CPU
    PatchManager         Jerarquía CONCERT → PATCH → LAYERS (+ variaciones estilo GP),
                         switching de patches seamless A/B con crossfade y patch-persist
    PluginRack           Rack de plugins (VST3/LV2) por layer
    MidiRouter           Entrada MIDI de todos los teclados + tap de control remoto
    MidiClockSync        Recepción de MIDI Clock/Start/Stop/Continue desde Ableton/DAW
    RemoteControl        OSC (JUCE OSC) + mapeo MIDI personalizable y persistente
    Persistence          Guarda/carga concerts (patches, layers, variaciones, mapeo) en XML
  ui/
    MainWindow           Ventana principal: patches, layers, medidores en vivo
```

## Jerarquía (punto intermedio entre Gig Performer y MainStage)

```
CONCERT (setlist)
 └── PATCH  = unión/superposición de 2+ sonidos en el mismo teclado
      ├── LAYER "Piano"    → rack VST3/LV2 + zona MIDI (key range/canal/transpose) + bus de salida
      ├── LAYER "Strings"  → idem  (rangos adyacentes = split; rangos solapados = layer)
      └── VARIACIONES (estilo GP): cambian gain/bypass de layers al instante, sin cambiar de patch
```

- **Patch = channel strips de MainStage / bloques de GP**: cada layer tiene su propio
  key range (split), canal MIDI, transposición, volumen/pan y **bus de salida propio**
  (p.ej. salidas 3-4 de la interfaz hacia la mesa), como en MainStage.
- **Switching de patches sin cortes**: A/B con crossfade en el límite de buffer + cola
  del patch anterior (patch persist: reverbs y notas sostenidas terminan de sonar).
- **Variaciones**: switch instantáneo dentro del mismo patch (estilo Variations de GP).
- **Multi-interfaz / hot-swap**: JACK permite cambiar de interfaz (M-TRACK DUO ↔ YC88/MX88)
  sin cerrar la app; el engine reengancha el callback solo.

## Funcionalidades objetivo (paridad con MainStage/Gig Performer)

- [x] **Patches**: cada patch = rack de instrumentos/efectos + ruteo MIDI + volumen/pan.
- [x] **MIDI → audio**: el `MidiRouter` recibe de todos los teclados, filtra por el patch
      activo (canal/transpose/split) y vuelca los mensajes al buffer del callback de audio.
- [x] **Mixto internos + laptop**: en la iglesia, el audio interno del YC88/MX88 sale por su
      interfaz USB y la laptop suma sus plugins por la misma interfaz; todo se mezcla en la mesa.
- [x] **Switching sin cortes**: sistema A/B — el patch siguiente se prepara con antelación
      (`prepareToPlay` fuera del hilo de audio) y el cambio ocurre en el límite de buffer con un
      crossfade corto (~5 ms). La cola del patch anterior (reverbs) se deja sonar unos buffers.
- [x] **Hot-swap de gear**: JACK permite reconectar dispositivos sin cerrar la app; el
      `AudioEngine` reacciona a cambios de dispositivo y reengancha sin dropouts.
- [x] **Sync con DAW**: MIDI Clock (24 ppqn), Start/Stop/Continue, Song Position Pointer;
      se reenvía a los plugins que sigan el transporte del host.
- [x] **Efectos y plugins**: VST3 + LV2 (formato nativo de Linux) vía `AudioPluginFormatManager`.
- [x] **Baja latencia**: JACK con buffer pequeño (ej. 128–256 samples @ 48 kHz), sin asignaciones
      en el hilo de audio.
- [x] **Control remoto**: OSC personalizable (`/patch/select <int>`, `/patch/next`, `/patch/prev`,
      `/rack/bypass <slot> <0|1>`, …) y mapeo MIDI (PC = índice de patch, CC 115/116 = ±1).
- [x] **Editor de rack**: añadir/quitar plugins VST3/LV2 por patch desde la UI, guardar/cargar
      el concert en XML (`*.livekeys.xml`).
- [x] **Medidor en vivo**: dispositivo, sample rate, buffer, latencia medida, CPU % y estado
      de sync MIDI Clock (BPM), refrescado cada 500 ms.
- [x] **Mapeo remoto configurable**: tabla MIDI (tecla → acción: siguiente/anterior/patch N/bypass)
      persistida en el concert junto a los patches.
- [x] **Estabilidad/recursos**: sin allocations en `processBlock`, plugins sueltos en hilo
      propio de Mensaje; recomendado i3 + 8 GB RAM.

## Compilar en Arch Linux

```bash
sudo pacman -S base-devel cmake ninja git \
    jack2 libx11-dev libxext-dev libxinerama-dev libxrandr-dev libxcursor-dev \
    libfreetype6-dev libfontconfig1-dev libasound2-dev libcurl4-openssl-dev \
    libwebkit2gtk-4.1-dev libgtk-3-dev mesa-libgl-dev libxss-dev pkg-config

cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

JUCE se descarga automáticamente vía CMake FetchContent (tag 7.0.12).

## Uso con JACK / PipeWire (recomendado en Arch)

```bash
# PipeWire con JACK habilitado (lo habitual hoy en Arch):
pw-jack livekeys        # o simplemente ejecuta la app; JUCE usa JACK si está disponible
# Buffer recomendado: qjackctl / jack_control -> 128 o 256 frames @ 48000 Hz
```

En casa: M-AUDIO M-TRACK DUO vía ALSA→JACK (zita-a2j o el bridge de PipeWire).
En la iglesia: YC88 / MX88 (su interfaz USB aparece como dispositivo JACK directamente).

## Hardware objetivo del usuario

- Casa: Yamaha YPG-235 (MIDI por USB, sin audio propio) + M-AUDIO M-TRACK DUO.
- Iglesia: Yamaha YC88 y MX88 (interfaz de audio USB integrada).
- Laptop: i5-8250U, 16 GB RAM, Arch Linux.
