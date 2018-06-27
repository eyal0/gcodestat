/*
 * File calcmove.c
 * Author: arhimed@gmail.com
 * Date 20070721
 *
 * TODO: support jerk (marlin)
 * TODO: support different limits on XYZE
 * TODO: reorganize code, group reusable segments
 *
 */


#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>
#include "calcmove.h"

double calcmove(char * buffer,  print_settings_t * print_settings){
  static double oldx = 0;        //old values required for sticky paremeters and other calcs
  static double oldy = 0;
  static double oldz = 0;
  static double olde = 0;
  static double oldf = 0;

  static double oldxa    = 0;    //vector of previous movement
  static double oldya    = 0;
  static double oldza    = 0;

  static double oldspeed = 0;    //speed previous move ended (start speed for current move)

  double ret;
  double distance;
  double costheta;
  double xa,ya,za;
  double sin_theta_d2; 
  double speed ;
  double x,y,z,e,f;
  char   c;
  double Ta,Td,Sa,Sd,Ts;

  speed = 0;
  char * buff;
  buff = buffer;

  //sticky
  f = oldf;
  x = oldx;
  y = oldy;
  z = oldz;
  e = olde;

  while('\0' != (c = *(buff++)) ){
    bool endofline;
    endofline = false;
    if ((c & 0xDF) == 'G'){
      while('\0' != (c = *(buff++)) ){
        switch (c){
          case 'X':
          case 'x':
            x = atof(buff++);
            break;
          case 'Y':
          case 'y':
            y = atof(buff++);
            break;
          case 'Z':
          case 'z':
            z = atof(buff++);
            break;
          case 'E':
          case 'e':
            e = atof(buff++);
            break;
          case 'F':
          case 'f':
            f = atof(buff++)/60.0; //convert speed to units/second from units/minute
            break;
          case ';':
          case '\n':
          case '\r':
            endofline = true;
        }
        if (endofline) break;
      }
      break;
    }
  }

  if (!print_settings->abs){
     x+=oldx;
     y+=oldy;
     z+=oldz;
  }
  if (!print_settings->eabs) e += olde;

  ret = 0.0;
  distance = sqrt(pow((x - oldx),2) + pow((y-oldy),2) + pow((z-oldz),2)); 
  if (distance == 0.0){
    oldf = f;
    return 0;
  }

  if (!print_settings->mm) distance = distance * 25.4; //if units are in inch then convert to mm


  // This move's change in x/y/z, aka delta x/y/z
  xa = x - oldx;
  ya = y - oldy;
  za = z - oldz;

  if (print_settings->jerk) {
    // Decompose oldspeed into components x/y/z
    // oldspeed's direction was the normalized vector of oldxa,oldyz,oldza
    double olddistance = sqrt(oldxa*oldxa + oldya*oldya + oldza*oldza);
    double oldspeed_x = oldspeed * oldxa / olddistance;
    double oldspeed_y = oldspeed * oldya / olddistance;
    double oldspeed_z = oldspeed * oldza / olddistance;

    // The speed that we want.
    double speed_x = f * xa / distance;
    double speed_y = f * ya / distance;
    double speed_z = f * za / distance;

    // Start with exit speed limited to full stop jerk.
    double speed_reduction_factor = 1;
    for (unsigned int i = 0; i < 3; i++) {
      double jerk, maxj;
      //TODO: jerk for each axis
      switch (i) {
        case 0: jerk = abs(speed_x), maxj = print_settings->jdev; break;
        case 1: jerk = abs(speed_y), maxj = print_settings->jdev; break;
        case 2: jerk = abs(speed_z), maxj = print_settings->jdev; break;
      }
      if (jerk > maxj) {
        speed_reduction_factor = _MIN_(speed_reduction_factor, maxj/jerk);
      }
    }
    double safe_speed = f * speed_reduction_factor;

    double v_factor = 1;
    double vmax_junction = _MIN_(f, oldf);
    const double smaller_speed_factor = vmax_junction / oldf;
    for (unsigned int axis = 0; axis < 3; axis++) {
      float v_exit, v_entry;
      switch (axis) {
        case 0: v_exit = oldspeed_x * smaller_speed_factor; v_entry = speed_x; break;
        case 1: v_exit = oldspeed_y * smaller_speed_factor; v_entry = speed_y; break;
        case 2: v_exit = oldspeed_z * smaller_speed_factor; v_entry = speed_z; break;
      }
      v_exit *= v_factor;
      v_entry *= v_factor;
      // Calculate jerk depending on whether the axis is coasting in the same direction or reversing.
      const float jerk = (v_exit > v_entry)
                         ? //                                  coasting             axis reversal
                         ( (v_entry > 0 || v_exit < 0) ? (v_exit - v_entry) : _MAX_(v_exit, -v_entry) )
                         : // v_exit <= v_entry                coasting             axis reversal
                         ( (v_entry < 0 || v_exit > 0) ? (v_entry - v_exit) : _MAX_(-v_exit, v_entry) );

      if (jerk > print_settings->jdev) {
        v_factor *= print_settings->jdev / jerk;
      }
    }
    vmax_junction *= v_factor;
    // Now the transition velocity is known, which maximizes the shared exit / entry velocity while
    // respecting the jerk factors, it may be possible, that applying separate safe exit / entry velocities will achieve faster prints.
    const float vmax_junction_threshold = vmax_junction * 0.99f;
    if (oldspeed > vmax_junction_threshold && safe_speed > vmax_junction_threshold) {
      vmax_junction = safe_speed;
    }
    speed = vmax_junction;
    printf("oldspeed %f, wantedspeed %f, junction %f\n", oldspeed, f, speed);
} else {
	  //SMOOTHIEWARE - JUNCTION DEVIATION & ACCELERATION

		costheta = (xa * oldxa + ya * oldya + za + oldza) / (sqrt(xa * xa + ya * ya + za * za) * sqrt(oldxa * oldxa + oldya * oldya + oldza * oldza));
		if (costheta < 0.95) {
			speed = f;
			if (costheta > -0.95F) {
				sin_theta_d2 = sqrt(0.5 * (1.0 - costheta));
				speed = _MIN_(speed, sqrt( print_settings->accel * print_settings->jdev * sin_theta_d2 / (1.0 - sin_theta_d2)));
			}
		}
		speed = _MIN_(speed, print_settings->x_maxspeed);
		speed = _MIN_(speed, print_settings->y_maxspeed);
  }
       Ta = (f - oldspeed) / print_settings->accel; // time to reach speed from start speed (end speed of previous move)
       Td = (f - speed) / print_settings->accel; // time to decelerate to "end speed" of current movement
       Sa = (f + oldspeed) * Ta / 2.0;    // length moved during acceleration
       Sd = (f + speed) * Td / 2.0;       // length moved during deceleration
       if ((Sa + Sd) < distance) {
         Ts = (distance - (Sa + Sd)) / f; // time spent during constant speed
       } else {
         Ts = 0; // no time for constant speed, move was too short for accel + decel + constant speed
         //split accel and decel in half, no clue how each firmware handles this but this seems like a proper thing to do
         //Sa = Sd == distance/2
         //T = 2*S / (V0+V1)
         Ta = distance / (f + oldspeed);
         Td = distance / (f + speed);
       }
       
       ret = Ta + Ts + Td;
       oldspeed = speed;


	oldx = x;
	oldy = y;
	oldz = z;
	olde = e;
	oldf = f;

	oldxa = xa;
	oldya = ya;
	oldza = za;

	return ret;
}
