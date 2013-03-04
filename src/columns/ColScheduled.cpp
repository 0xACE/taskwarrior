////////////////////////////////////////////////////////////////////////////////
// taskwarrior - a command line task list manager.
//
// Copyright 2006-2013, Paul Beckingham, Federico Hernandez.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
// THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//
// http://www.opensource.org/licenses/mit-license.php
//
////////////////////////////////////////////////////////////////////////////////

#define L10N                                           // Localization complete.

#include <stdlib.h>
#include <Context.h>
#include <ColScheduled.h>
#include <Date.h>
#include <Duration.h>
#include <text.h>
#include <i18n.h>

extern Context context;

////////////////////////////////////////////////////////////////////////////////
ColumnScheduled::ColumnScheduled ()
{
  _name      = "scheduled";
  _label     = STRING_COLUMN_LABEL_SCHED;

  _styles.push_back ("countdown");

  Date now;
  now += 125;
  _examples.push_back (Duration (now - Date ()).formatCompact ());
}

////////////////////////////////////////////////////////////////////////////////
ColumnScheduled::~ColumnScheduled ()
{
}

////////////////////////////////////////////////////////////////////////////////
bool ColumnScheduled::validate (std::string& value)
{
  return ColumnDate::validate (value);
}

////////////////////////////////////////////////////////////////////////////////
// Overriden so that style <----> label are linked.
// Note that you can not determine which gets called first.
void ColumnScheduled::setStyle (const std::string& value)
{
  _style = value;

  if (_style == "countdown" && _label == STRING_COLUMN_LABEL_DUE)
    _label = STRING_COLUMN_LABEL_COUNT;
}

////////////////////////////////////////////////////////////////////////////////
// Set the minimum and maximum widths for the value.
void ColumnScheduled::measure (Task& task, unsigned int& minimum, unsigned int& maximum)
{
  minimum = maximum = 0;

  if (task.has (_name))
  {
    if (_style == "countdown")
    {
      Date date ((time_t) strtol (task.get (_name).c_str (), NULL, 10));
      Date now;
      minimum = maximum = Duration (now - date).format ().length ();
    }
    else
      ColumnDate::measure (task, minimum, maximum);
  }
}

////////////////////////////////////////////////////////////////////////////////
void ColumnScheduled::render (
  std::vector <std::string>& lines,
  Task& task,
  int width,
  Color& color)
{
  if (task.has (_name))
  {
    if (_style == "countdown")
    {
      Date date ((time_t) strtol (task.get (_name).c_str (), NULL, 10));
      Date now;

      lines.push_back (
        color.colorize (
          rightJustify (
            Duration (now - date).format (), width)));
    }
    else
      ColumnDate::render (lines, task, width, color);
  }
}

////////////////////////////////////////////////////////////////////////////////
