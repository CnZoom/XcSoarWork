/*
Copyright_License {

  XCSoar Glide Computer - http://www.xcsoar.org/
  Copyright (C) 2000-2011 The XCSoar Project
  A detailed list of copyright holders can be found in the file "AUTHORS".

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License
  as published by the Free Software Foundation; either version 2
  of the License, or (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
}
*/

#include "WayPointFileSeeYou.hpp"
#include "Units.hpp"
#include "Waypoint/Waypoints.hpp"

#include <stdio.h>

bool
WayPointFileSeeYou::parseLine(const TCHAR* line, const unsigned linenum,
                              Waypoints &way_points, 
                              const RasterTerrain *terrain)
{
  TCHAR ctemp[255];
  const TCHAR *params[20];
  static const unsigned int max_params=sizeof(params)/sizeof(params[0]);
  size_t n_params;

  static unsigned iName = 0, iCode = 1, iCountry = 2;
  static unsigned iLatitude = 3, iLongitude = 4, iElevation = 5;
  static unsigned iStyle = 6, iRWDir = 7, iRWLen = 8;
  static unsigned iFrequency = 9, iDescription = 10;

  static bool ignore_following = false;

  // If (end-of-file or comment)
  if (line[0] == '\0' || line[0] == 0x1a ||
      _tcsstr(line, _T("**")) == line ||
      _tcsstr(line, _T("*")) == line)
    // -> return without error condition
    return true;

  if (_tcslen(line) >= sizeof(ctemp) / sizeof(ctemp[0]))
    /* line too long for buffer */
    return false;

  // Parse first line holding field order
  /// @todo linenum == 0 should be the first
  /// (not ignored) line, not just line 0
  if (linenum == 0) {
    // Get fields
    n_params = extractParameters(line, ctemp, params, max_params, true, _T('"'));

    // Iterate through fields and save the field order
    for (unsigned i = 0; i < n_params; i++) {
      const TCHAR* value = params[i];

      if (!_tcscmp(value, _T("name")))
        iName = i;
      else if (!_tcscmp(value, _T("code")))
        iCode = i;
      else if (!_tcscmp(value, _T("country")))
        iCountry = i;
      else if (!_tcscmp(value, _T("lat")))
        iLatitude = i;
      else if (!_tcscmp(value, _T("lon")))
        iLongitude = i;
      else if (!_tcscmp(value, _T("elev")))
        iElevation = i;
      else if (!_tcscmp(value, _T("style")))
        iStyle = i;
      else if (!_tcscmp(value, _T("rwdir")))
        iRWDir = i;
      else if (!_tcscmp(value, _T("rwlen")))
        iRWLen = i;
      else if (!_tcscmp(value, _T("freq")))
        iFrequency = i;
      else if (!_tcscmp(value, _T("desc")))
        iDescription = i;
    }
    ignore_following = false;

    return true;
  }

  // If task marker is reached ignore all following lines
  if (_tcsstr(line, _T("-----Related Tasks-----")) == line)
    ignore_following = true;
  if (ignore_following)
    return true;

  // Get fields
  n_params = extractParameters(line, ctemp, params, max_params, true, _T('"'));

  // Check if the basic fields are provided
  if (iName >= n_params)
    return false;
  if (iLatitude >= n_params)
    return false;
  if (iLongitude >= n_params)
    return false;

  GeoPoint location;

  // Latitude (e.g. 5115.900N)
  if (!parseAngle(params[iLatitude], location.Latitude, true))
    return false;

  // Longitude (e.g. 00715.900W)
  if (!parseAngle(params[iLongitude], location.Longitude, false))
    return false;

  location.normalize(); // ensure longitude is within -180:180

  Waypoint new_waypoint(location);
  new_waypoint.FileNum = file_num;

  // Name (e.g. "Some Turnpoint")
  if (*params[iName] == _T('\0'))
    return false;
  new_waypoint.Name = params[iName];

  // Elevation (e.g. 458.0m)
  /// @todo configurable behaviour
  bool alt_ok = iElevation < n_params &&
    parseAltitude(params[iElevation], new_waypoint.Altitude);
  check_altitude(new_waypoint, terrain, alt_ok);

  // Style (e.g. 5)
  /// @todo include peaks with peak symbols etc.
  if (iStyle < n_params)
    parseStyle(params[iStyle], new_waypoint.Flags);

  // Runway length (e.g. 546.0m)
  fixed rwlen;
  if (iRWLen < n_params && parseDistance(params[iRWLen], rwlen))
    new_waypoint.RunwayLength = rwlen;
  else {
    new_waypoint.RunwayLength = 0;
    rwlen = fixed_zero;
  }

  // If the Style attribute did not state that this is an airport
  if (!new_waypoint.Flags.Airport) {
    // If runway length is between 100m and 300m -> landpoint
    if (rwlen > fixed(100) && rwlen <= fixed(300))
      new_waypoint.Flags.LandPoint = true;
    // If runway length is higher then 300m -> airport
    if (rwlen > fixed(300))
      new_waypoint.Flags.Airport = true;
  }

  // Frequency & runway direction/length (for airports and landables)
  // and description (e.g. "Some Description")
  if (new_waypoint.is_landable()) {
    if (iFrequency < n_params)
      appendStringWithSeperator(new_waypoint.Comment, params[iFrequency]);

    if (iRWDir < n_params && *params[iRWDir]) {
      appendStringWithSeperator(new_waypoint.Comment, params[iRWDir]);
      new_waypoint.Comment += _T("°");
      int direction =_tcstol(params[iRWDir], NULL, 10);
      if (direction == 360)
        direction = 0;
      else if (direction < 0 || direction > 360)
        direction = -1;
      new_waypoint.RunwayDirection = Angle::degrees(fixed(direction));
    }

    if (iRWLen < n_params)
      appendStringWithSeperator(new_waypoint.Comment, params[iRWLen]);
  }
  if (iDescription < n_params)
    appendStringWithSeperator(new_waypoint.Comment, params[iDescription]);

  add_waypoint(way_points, new_waypoint);
  return true;
}

/**
  * Append string to another inserting seperator character if dest is not empty
  * @param dest result string
  * @param src the string to append
  * @param seperator character (default: ' ')
  */
void
WayPointFileSeeYou::appendStringWithSeperator(tstring &dest,
                                              const TCHAR* src,
                                              const TCHAR seperator)
{
  if (*src == _T('\0'))
    return;
  if (dest.length() > 0)
    dest += seperator;
  dest += src;
}

bool
WayPointFileSeeYou::parseAngle(const TCHAR* src, Angle& dest, const bool lat)
{
  TCHAR *endptr;

  long min = _tcstol(src, &endptr, 10);
  if (endptr == src || *endptr != _T('.') || min < 0)
    return false;

  src = endptr + 1;

  long deg = min / 100;
  min = min % 100;
  if (min >= 60)
    return false;

  // Limit angle to +/- 90 degrees for Latitude or +/- 180 degrees for Longitude
  deg = std::min(deg, lat ? 90L : 180L);

  long l = _tcstol(src, &endptr, 10);
  if (endptr != src + 3 || l < 0 || l >= 1000)
    return false;

  fixed value = fixed(deg) + fixed(min) / 60 + fixed(l) / 60000;

  TCHAR sign = *endptr;
  if (sign == 'W' || sign == 'w' || sign == 'S' || sign == 's')
    value = -value;

  // Save angle
  dest = Angle::degrees(value);
  return true;
}

bool
WayPointFileSeeYou::parseAltitude(const TCHAR* src, fixed& dest)
{
  // Parse string
  TCHAR *endptr;
  double value = _tcstod(src, &endptr);
  if (endptr == src)
    return false;

  dest = fixed(value);

  // Convert to system unit if necessary
  TCHAR unit = *endptr;
  if (unit == 'F' || unit == 'f')
    dest = Units::ToSysUnit(dest, unFeet);

  // Save altitude
  return true;
}

bool
WayPointFileSeeYou::parseDistance(const TCHAR* src, fixed& dest)
{
  // Parse string
  TCHAR *endptr;
  double value = _tcstod(src, &endptr);
  if (endptr == src)
    return false;

  dest = fixed(value);

  // Convert to system unit if necessary, assume m as default
  TCHAR* unit = endptr;
  if (_tcsicmp(unit, _T("ml")) == 0)
    dest = Units::ToSysUnit(dest, unStatuteMiles);
  else if (_tcsicmp(unit, _T("nm")) == 0)
    dest = Units::ToSysUnit(dest, unNauticalMiles);

  // Save distance
  return true;
}

bool
WayPointFileSeeYou::parseStyle(const TCHAR* src, WaypointFlags& dest)
{
  // 1 - Normal
  // 2 - AirfieldGrass
  // 3 - Outlanding
  // 4 - GliderSite
  // 5 - AirfieldSolid ...

  // Parse string
  TCHAR *endptr;
  long style = _tcstol(src, &endptr, 10);
  if (endptr == src)
    return false;

  // Update flags
  dest.LandPoint = (style == 3);
  dest.Airport = (style == 2 || style == 4 || style == 5);
  dest.TurnPoint = true;

  return true;
}
