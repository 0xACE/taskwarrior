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

#ifndef INCLUDED_COMMAND
#define INCLUDED_COMMAND

#include <map>
#include <vector>
#include <string>
#include <Task.h>
#include <A3.h>

class Command
{
public:
  Command ();
  Command (const Command&);
  Command& operator= (const Command&);
  bool operator== (const Command&) const;     // TODO Is this necessary?
  virtual ~Command ();

  static void factory (std::map <std::string, Command*>&);

  std::string keyword () const;
  std::string usage () const;
  std::string description () const;
  bool read_only () const;
  bool displays_id () const;
  virtual int execute (std::string&) = 0;

protected:
  void filter (const std::vector <Task>&, std::vector <Task>&);
  void filter (std::vector <Task>&);
  bool filter_shortcut (const A3&);

  void modify_task_description_replace (Task&, const A3&);
  void modify_task_description_prepend (Task&, const A3&);
  void modify_task_description_append (Task&, const A3&);
  void modify_task_annotate (Task&, const A3&);
  void modify_task (Task&, const A3&, std::string&);

  void safety ();
  bool permission (const Task&, const std::string&, unsigned int);

protected:
  std::string _keyword;
  std::string _usage;
  std::string _description;
  bool        _read_only;
  bool        _displays_id;
  bool        _needs_confirm;

  // Permission support
  bool        _permission_quit;
  bool        _permission_all;
  bool        _first_iteration;
};

#endif
////////////////////////////////////////////////////////////////////////////////
