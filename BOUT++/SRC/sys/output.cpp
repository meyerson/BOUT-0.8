/**************************************************************************
 * Output, for printing messages/errors etc.
 *
 **************************************************************************
 * Copyright 2010 B.D.Dudson, S.Farley, M.V.Umansky, X.Q.Xu
 *
 * Contact: Ben Dudson, bd512@york.ac.uk
 * 
 * This file is part of BOUT++.
 *
 * BOUT++ is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * BOUT++ is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with BOUT++.  If not, see <http://www.gnu.org/licenses/>.
 *
 **************************************************************************/

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "output.h"

void Output::enable()
{ 
  add(std::cout);
  enabled = true;
}

void Output::disable()
{
  remove(std::cout);
  enabled = false;
}

int Output::open(const char* fname, ...)
{
  va_list ap;  // List of arguments
  
  if(fname == (const char*) NULL)
    return 1;

  va_start(ap, fname);
    vsprintf(buffer, fname, ap);
  va_end(ap);

  close();

  file.open(buffer);

  if(!file.is_open()) {
    fprintf(stderr, "Could not open log file '%s'\n", buffer);
    return 1;
  }

  add(file);

  return 0;
}

void Output::close()
{
  if(!file.is_open())
    return;
  
  remove(file);
  file.close();
}

void Output::write(const char* string, ...)
{
  va_list ap;  // List of arguments

  if(string == (const char*) NULL)
    return;
  
  va_start(ap, string);
    vsprintf(buffer, string, ap);
  va_end(ap);

  multioutbuf_init::buf()->sputn(buffer, strlen(buffer));
}

void Output::print(const char* string, ...)
{
  va_list ap;  // List of arguments

  if(!enabled)
    return; // Only output if to screen

  if(string == (const char*) NULL)
    return;
  
  va_start(ap, string);
    vprintf(string, ap);
  va_end(ap);
  
  fflush(stdout);
}
