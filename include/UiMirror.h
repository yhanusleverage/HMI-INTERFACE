#ifndef UI_MIRROR_H
#define UI_MIRROR_H

/** Espejo Serial de lo que el usuario ve en el HMI. */
namespace UiMirror {
void dump();           // una captura ASCII
void setAuto(bool on); // cada ~2 s en loop
bool autoOn();
void tick(unsigned long nowMs);
}

#endif
