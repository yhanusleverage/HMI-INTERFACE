#ifndef PROTOCOL_H
#define PROTOCOL_H

/** Tramas JSON por línea (\n) entre display S3 y ESP32 master. */

/*
Master -> Display:
{"t":"telemetry","ph":5.8,"ec":470,"temp_agua":22.0,"orp":362,"do":9.1}

Display -> Master:
{"t":"cmd","action":"setpoint","ph":5.8}
{"t":"cmd","action":"setpoint","ec":470}
{"t":"cmd","action":"setpoint","temp_agua":22.0}
{"t":"cmd","action":"setpoint","orp":360}
{"t":"cmd","action":"setpoint","do":9.0}
{"t":"cmd","action":"calib","param":"ph","point":7.0}
{"t":"cmd","action":"calib","param":"orp","point":220.0}
{"t":"cmd","action":"calib","param":"do","point":8.26}

Dosificacion manual:
{"t":"cmd","action":"dose","channel":"A","ml":5.0}
{"t":"cmd","action":"dose","channel":"B","ml":2.0}
{"t":"cmd","action":"dose","channel":"pH_down","ml":1.0}
{"t":"cmd","action":"dose","channel":"pH_up","ml":1.0}
{"t":"cmd","action":"dose_stop","channel":"A"}
{"t":"cmd","action":"dose_hold","channel":"A","on":1}
*/

#endif
