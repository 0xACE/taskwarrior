////////////////////////////////////////////////////////////////////////////////
//
// Copyright 2006 - 2015, Paul Beckingham, Federico Hernandez.
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

#include <cmake.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <Context.h>
#include <FS.h>
#include <Eval.h>
#include <Variant.h>
#include <text.h>
#include <util.h>
#include <main.h>
#include <i18n.h>
#ifdef HAVE_COMMIT
#include <commit.h>
#endif

// Supported modifiers, synonyms on the same line.
static const char* modifierNames[] =
{
  "before",     "under",    "below",
  "after",      "over",     "above",
  "none",
  "any",
  "is",         "equals",
  "isnt",       "not",
  "has",        "contains",
  "hasnt",
  "startswith", "left",
  "endswith",   "right",
  "word",
  "noword"
};

#define NUM_MODIFIER_NAMES       (sizeof (modifierNames) / sizeof (modifierNames[0]))

////////////////////////////////////////////////////////////////////////////////
Context::Context ()
: rc_file ("~/.taskrc")
, data_dir ("~/.task")
, config ()
, tdb2 ()
, dom ()
, determine_color_use (true)
, use_color (true)
, run_gc (true)
, verbosity_legacy (false)
, terminal_width (0)
, terminal_height (0)
{
}

////////////////////////////////////////////////////////////////////////////////
Context::~Context ()
{
  for (auto& com : commands)
    delete com.second;

  for (auto& col : columns)
    delete col.second;
}

////////////////////////////////////////////////////////////////////////////////
int Context::initialize (int argc, const char** argv)
{
  timer_init.start ();
  int rc = 0;

  try
  {
    ////////////////////////////////////////////////////////////////////////////
    //
    // [1] Load the correct config file.
    //     - Default to ~/.taskrc (ctor).
    //     - Allow command line override rc:<file>
    //     - Allow $TASKRC override.
    //     - Load resultant file.
    //     - Apply command line overrides to the config.
    //
    ////////////////////////////////////////////////////////////////////////////

    CLI2::getOverride (argc, argv, home_dir, rc_file);
    CLI::getOverride (argc, argv, home_dir, rc_file);

    char* override = getenv ("TASKRC");
    if (override)
    {
      rc_file = File (override);
      header (format (STRING_CONTEXT_RC_OVERRIDE, rc_file._data));
    }

    config.clear ();
    config.load (rc_file);
    CLI2::applyOverrides (argc, argv);
    CLI::applyOverrides (argc, argv);

    ////////////////////////////////////////////////////////////////////////////
    //
    // [2] Locate the data directory.
    //     - Default to ~/.task (ctor).
    //     - Allow command line override rc.data.location:<dir>
    //     - Allow $TASKDATA override.
    //     - Inform TDB2 where to find data.
    //     - Create the rc_file and data_dir, if necessary.
    //
    ////////////////////////////////////////////////////////////////////////////

    CLI2::getDataLocation (argc, argv, data_dir);
    CLI::getDataLocation (argc, argv, data_dir);

    override = getenv ("TASKDATA");
    if (override)
    {
      data_dir = Directory (override);
      config.set ("data.location", data_dir._data);
      header (format (STRING_CONTEXT_DATA_OVERRIDE, data_dir._data));
    }

    tdb2.set_location (data_dir);
    createDefaultConfig ();

    ////////////////////////////////////////////////////////////////////////////
    //
    // [3] Instantiate Command objects and capture entities.
    //
    ////////////////////////////////////////////////////////////////////////////

    Command::factory (commands);
    for (auto& cmd : commands)
    {
      cli.entity ("cmd", cmd.first);
      cli.entity ((cmd.second->read_only () ? "readcmd" : "writecmd"), cmd.first);

      if (cmd.first[0] == '_')
        cli.entity ("helper", cmd.first);
    }

    ////////////////////////////////////////////////////////////////////////////
    //
    // [4] Instantiate Column objects and capture entities.
    //
    ////////////////////////////////////////////////////////////////////////////

    Column::factory (columns);
    for (auto& col : columns)
      cli.entity ("attribute", col.first);

    cli.entity ("pseudo", "limit");

    ////////////////////////////////////////////////////////////////////////////
    //
    // [5] Capture modifier and operator entities.
    //
    ////////////////////////////////////////////////////////////////////////////

    for (unsigned int i = 0; i < NUM_MODIFIER_NAMES; ++i)
      cli.entity ("modifier", modifierNames[i]);

    for (auto& op : Eval::getOperators ())
      cli.entity ("operator", op);

    for (auto& op : Eval::getBinaryOperators ())
      cli.entity ("binary_operator", op);

    ////////////////////////////////////////////////////////////////////////////
    //
    // [6] Complete the Context initialization.
    //
    ////////////////////////////////////////////////////////////////////////////

    initializeColorRules ();
    staticInitialization ();
    propagateDebug ();
    loadAliases ();

    ////////////////////////////////////////////////////////////////////////////
    //
    // [7] Parse the command line.
    //
    ////////////////////////////////////////////////////////////////////////////

    cli.initialize (argc, argv);
    cli.analyze (true, true);

    // Extract a recomposed command line.
    bool foundDefault = false;
    bool foundAssumed = false;
    std::string combined;
    for (auto& a : cli._args)
    {
      if (combined.length ())
        combined += ' ';

      if (a.attribute ("canonical") != "")
        combined += a.attribute ("canonical");
      else
        combined += a.attribute ("raw");

      if (a.hasTag ("DEFAULT"))
        foundDefault = true;

      if (a.hasTag ("ASSUMED"))
        foundAssumed = true;
    }

    if (foundDefault)
      header ("[" + combined + "]");

    if (foundAssumed)
      header (STRING_ASSUME_INFO);

    ////////////////////////////////////////////////////////////////////////////
    //
    // [8] Initialize hooks.
    //
    ////////////////////////////////////////////////////////////////////////////

    hooks.initialize ();
  }

  catch (const std::string& message)
  {
    error (message);
    rc = 2;
  }

  catch (int)
  {
    // Hooks can terminate processing by throwing integers.
    rc = 4;
  }

  catch (...)
  {
    error (STRING_UNKNOWN_ERROR);
    rc = 3;
  }

  // On initialization failure...
  if (rc)
  {
    // Dump all debug messages, controlled by rc.debug.
    if (config.getBoolean ("debug"))
    {
      for (auto& d : debugMessages)
        if (color ())
          std::cerr << colorizeDebug (d) << "\n";
        else
          std::cerr << d << "\n";
    }

    // Dump all headers, controlled by 'header' verbosity token.
    if (verbose ("header"))
    {
      for (auto& h : headers)
        if (color ())
          std::cerr << colorizeHeader (h) << "\n";
        else
          std::cerr << h << "\n";
    }

    // Dump all footnotes, controlled by 'footnote' verbosity token.
    if (verbose ("footnote"))
    {
      for (auto& f : footnotes)
        if (color ())
          std::cerr << colorizeFootnote (f) << "\n";
        else
          std::cerr << f << "\n";
    }

    // Dump all errors, non-maskable.
    // Colorized as footnotes.
    for (auto& e : errors)
      if (color ())
        std::cerr << colorizeFootnote (e) << "\n";
      else
        std::cerr << e << "\n";
  }

  timer_init.stop ();
  return rc;
}

////////////////////////////////////////////////////////////////////////////////
int Context::run ()
{
  int rc;
  std::string output;

  try
  {
    hooks.onLaunch ();
    rc = dispatch (output);
    tdb2.commit ();           // Harmless if called when nothing changed.
    hooks.onExit ();          // No chance to update data.

    std::stringstream s;
    s << "Perf "
      << PACKAGE_STRING
      << " "
#ifdef HAVE_COMMIT
      << COMMIT
#else
      << "-"
#endif
      << " "
      << Date ().toISO ()

      << " init:"   << timer_init.total ()
      << " load:"   << timer_load.total ()
      << " gc:"     << timer_gc.total ()
      << " filter:" << timer_filter.total ()
      << " commit:" << timer_commit.total ()
      << " sort:"   << timer_sort.total ()
      << " render:" << timer_render.total ()
      << " hooks:"  << timer_hooks.total ()
      << " total:"  << (timer_init.total ()   +
                        timer_load.total ()   +
                        timer_gc.total ()     +
                        timer_filter.total () +
                        timer_commit.total () +
                        timer_sort.total ()   +
                        timer_render.total () +
                        timer_hooks.total ())
      << "\n";
    debug (s.str ());
  }

  catch (const std::string& message)
  {
    error (message);
    rc = 2;
  }

  catch (int)
  {
    // Hooks can terminate processing by throwing integers.
    rc = 4;
  }

  catch (...)
  {
    error (STRING_UNKNOWN_ERROR);
    rc = 3;
  }

  // Dump all debug messages, controlled by rc.debug.
  if (config.getBoolean ("debug"))
  {
    for (auto& d : debugMessages)
      if (color ())
        std::cerr << colorizeDebug (d) << "\n";
      else
        std::cerr << d << "\n";
  }

  // Dump all headers, controlled by 'header' verbosity token.
  if (verbose ("header"))
  {
    for (auto& h : headers)
      if (color ())
        std::cerr << colorizeHeader (h) << "\n";
      else
        std::cerr << h << "\n";
  }

  // Dump the report output.
  std::cout << output;

  // Dump all footnotes, controlled by 'footnote' verbosity token.
  if (verbose ("footnote"))
  {
    for (auto& f : footnotes)
      if (color ())
        std::cerr << colorizeFootnote (f) << "\n";
      else
        std::cerr << f << "\n";
  }

  // Dump all errors, non-maskable.
  // Colorized as footnotes.
  for (auto& e : errors)
    if (color ())
      std::cerr << colorizeError (e) << "\n";
    else
      std::cerr << e << "\n";

  return rc;
}

////////////////////////////////////////////////////////////////////////////////
// Dispatch to the command found by the CLI parser.
int Context::dispatch (std::string &out)
{
  // Autocomplete args against keywords.
  std::string command = cli.getCommand ();
  if (command != "")
  {
    updateXtermTitle ();
    updateVerbosity ();

    Command* c = commands[command];
    assert (c);

    // GC is invoked prior to running any command that displays task IDs, if
    // possible.
    if (c->displays_id () && !tdb2.read_only ())
    {
      run_gc = config.getBoolean ("gc");
      tdb2.gc ();
    }
    else
    {
      run_gc = false;
    }

/*
    // Only read-only commands can be run when TDB2 is read-only.
    // TODO Implement TDB2::read_only
    if (tdb2.read_only () && !c->read_only ())
      throw std::string ("");
*/

    return c->execute (out);
  }

  assert (commands["help"]);
  return commands["help"]->execute (out);
}

////////////////////////////////////////////////////////////////////////////////
bool Context::color ()
{
#ifdef FEATURE_COLOR
  if (determine_color_use)
  {
    // What the config says.
    use_color = config.getBoolean ("color");

    // Only tty's support color.
    if (! isatty (STDOUT_FILENO))
    {
      // No ioctl.
      config.set ("detection", "off");
      config.set ("color",     "off");

      // Files don't get color.
      use_color = false;
    }

    // Override.
    if (config.getBoolean ("_forcecolor"))
    {
      config.set ("color", "on");
      use_color = true;
    }

    // No need to go through this again.
    determine_color_use = false;
  }

  // Cached result.
  return use_color;
#else
  return false;
#endif
}

////////////////////////////////////////////////////////////////////////////////
// Support verbosity levels:
//
//   rc.verbose=1          Show all feedback.
//   rc.verbose=0          Show regular feedback.
//   rc.verbose=nothing    Show the absolute minimum.
//   rc.verbose=one,two    Show verbosity for 'one' and 'two' only.
//
// TODO This mechanism is clunky, and should slowly evolve into something more
//      logical and consistent.  This should probably mean that 'nothing' should
//      take the place of '0'.
bool Context::verbose (const std::string& token)
{
  if (verbosity.empty ())
  {
    verbosity_legacy = config.getBoolean ("verbose");
    split (verbosity, config.get ("verbose"), ',');

    // Regular feedback means almost everything.
    // This odd test is to see if a Boolean-false value is a real one, which
    // means it is not 1/true/T/yes/on, but also should not be one of the
    // valid tokens either.
    if (!verbosity_legacy && ! verbosity.empty ())
    {
      std::string v = *(verbosity.begin ());
      if (v != "nothing"  &&
          v != "blank"    &&  // This list must be complete.
          v != "header"   &&  //
          v != "footnote" &&  //
          v != "label"    &&  //
          v != "new-id"   &&  //
          v != "new-uuid" &&  //
          v != "affected" &&  //
          v != "edit"     &&  //
          v != "special"  &&  //
          v != "project"  &&  //
          v != "sync"     &&  //
          v != "filter")      //
      {
        // This list emulates rc.verbose=off in version 1.9.4.
        verbosity = {"blank", "label", "new-id", "edit"};
      }
    }

    // Some flags imply "footnote" verbosity being active.  Make it so.
    if (! verbosity.count ("footnote"))
    {
      // TODO: Some of these may not use footnotes yet.  They should.
      for (auto flag : {"affected", "new-id", "new-uuid", "project"})
      {
        if (verbosity.count (flag))
        {
          verbosity.insert ("footnote");
          break;
        }
      }
    }
  }

  // rc.verbose=true|y|yes|1|on overrides all.
  if (verbosity_legacy)
    return true;

  // rc.verbose=nothing overrides all.
  if (verbosity.size () == 1 &&
      *(verbosity.begin ()) == "nothing")
    return false;

  // Specific token match.
  if (verbosity.count (token))
    return true;

  return false;
}

////////////////////////////////////////////////////////////////////////////////
const std::vector <std::string> Context::getColumns () const
{
  std::vector <std::string> output;
  for (auto& col : columns)
    output.push_back (col.first);

  return output;
}

////////////////////////////////////////////////////////////////////////////////
const std::vector <std::string> Context::getCommands () const
{
  std::vector <std::string> output;
  for (auto& cmd : commands)
    output.push_back (cmd.first);

  return output;
}

////////////////////////////////////////////////////////////////////////////////
// A value of zero mean unlimited.
// A value of 'page' means however many screen lines there are.
// A value of a positive integer is a row/task limit.
void Context::getLimits (int& rows, int& lines)
{
  rows = 0;
  lines = 0;

  // This is an integer specified as a filter (limit:10).
  std::string limit = cli.getLimit ();
  if (limit != "")
  {
    if (limit == "page")
    {
      rows = 0;
      lines = getHeight ();
    }
    else
    {
      rows = (int) strtol (limit.c_str (), NULL, 10);
      lines = 0;
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
// The 'Task' object, among others, is shared between projects.  To make this
// easier, it has been decoupled from Context.
void Context::staticInitialization ()
{
  CLI::minimumMatchLength   = config.getInteger ("abbreviation.minimum");
  CLI2::minimumMatchLength   = config.getInteger ("abbreviation.minimum");

  Task::defaultProject      = config.get ("default.project");
  Task::defaultDue          = config.get ("default.due");

  Task::searchCaseSensitive = Variant::searchCaseSensitive = config.getBoolean ("search.case.sensitive");
  Task::regex               = Variant::searchUsingRegex    = config.getBoolean ("regex");
  Lexer::dateFormat         = Variant::dateFormat          = config.get ("dateformat");
  Lexer::isoEnabled         = Variant::isoEnabled          = config.getBoolean ("date.iso");

  for (auto& rc : config)
  {
    if (rc.first.substr (0, 4) == "uda." &&
        rc.first.substr (rc.first.length () - 7, 7) == ".values")
    {
      std::string name = rc.first.substr (4, rc.first.length () - 7 - 4);
      std::vector <std::string> values;
      split (values, rc.second, ',');

      for (auto r = values.rbegin(); r != values.rend (); ++r)
        Task::customOrder[name].push_back (*r);
    }
  }

  for (auto& col : columns)
    Task::attributes[col.first] = col.second->type ();

  Task::urgencyProjectCoefficient     = config.getReal ("urgency.project.coefficient");
  Task::urgencyActiveCoefficient      = config.getReal ("urgency.active.coefficient");
  Task::urgencyScheduledCoefficient   = config.getReal ("urgency.scheduled.coefficient");
  Task::urgencyWaitingCoefficient     = config.getReal ("urgency.waiting.coefficient");
  Task::urgencyBlockedCoefficient     = config.getReal ("urgency.blocked.coefficient");
  Task::urgencyAnnotationsCoefficient = config.getReal ("urgency.annotations.coefficient");
  Task::urgencyTagsCoefficient        = config.getReal ("urgency.tags.coefficient");
  Task::urgencyNextCoefficient        = config.getReal ("urgency.next.coefficient");
  Task::urgencyDueCoefficient         = config.getReal ("urgency.due.coefficient");
  Task::urgencyBlockingCoefficient    = config.getReal ("urgency.blocking.coefficient");
  Task::urgencyAgeCoefficient         = config.getReal ("urgency.age.coefficient");
  Task::urgencyAgeMax                 = config.getReal ("urgency.age.max");

  // Tag- and project-specific coefficients.
  std::vector <std::string> all;
  config.all (all);
  for (auto& var : all)
    if (var.substr (0, 13) == "urgency.user." ||
        var.substr (0, 12) == "urgency.uda.")
      Task::coefficients[var] = config.getReal (var);
}

////////////////////////////////////////////////////////////////////////////////
void Context::createDefaultConfig ()
{
  // Do we need to create a default rc?
  if (! rc_file.exists ())
  {
    if (config.getBoolean ("confirmation") &&
        !confirm (format (STRING_CONTEXT_CREATE_RC, home_dir, rc_file._data)))
      throw std::string (STRING_CONTEXT_NEED_RC);

    config.createDefaultRC (rc_file, data_dir._original);
  }

  // Create data location, if necessary.
  config.createDefaultData (data_dir);
}

////////////////////////////////////////////////////////////////////////////////
void Context::decomposeSortField (
  const std::string& field,
  std::string& key,
  bool& ascending,
  bool& breakIndicator)
{
  int length = field.length ();

  int decoration = 1;
  breakIndicator = false;
  if (field[length - decoration] == '/')
  {
    breakIndicator = true;
    ++decoration;
  }

  if (field[length - decoration] == '+')
  {
    ascending = true;
    key = field.substr (0, length - decoration);
  }
  else if (field[length - decoration] == '-')
  {
    ascending = false;
    key = field.substr (0, length - decoration);
  }
  else
  {
    ascending = true;
    key = field;
  }
}

////////////////////////////////////////////////////////////////////////////////
// Note: The reason some of these are commented out is because the ::clear
// method is not really "clear" but "clear_some".  Some members do not need to
// be initialized.  That makes this method something of a misnomer.  So be it.
//
// TODO Is this method used anywhere?
void Context::clear ()
{
  tdb2.clear ();

  // Eliminate the command objects.
  for (auto& cmd : commands)
    delete cmd.second;

  commands.clear ();

  // Eliminate the column objects.
  for (auto& col : columns)
    delete col.second;

  columns.clear ();
  clearMessages ();
}

////////////////////////////////////////////////////////////////////////////////
// This capability is to answer the question of 'what did I just do to generate
// this output?'.
void Context::updateXtermTitle ()
{
  if (config.getBoolean ("xterm.title") && isatty (STDOUT_FILENO))
  {
    std::string command = cli.getCommand ();
    std::string title;

    for (auto a = cli._args.begin (); a != cli._args.end (); ++a)
    {
      if (a != cli._args.begin ())
        title += ' ';

      title += a->attribute ("raw");
    }

    std::cout << "]0;task " << command << " " << title << "";
  }
}

////////////////////////////////////////////////////////////////////////////////
// This function allows a clean output if the command is a helper subcommand.
void Context::updateVerbosity ()
{
  std::string command = cli.getCommand ();
  if (command != "" &&
      command[0] == '_')
  {
    verbosity = {"nothing"};
  }
}

////////////////////////////////////////////////////////////////////////////////
void Context::loadAliases ()
{
  for (auto& i : config)
    if (i.first.substr (0, 6) == "alias.")
      cli.alias (i.first.substr (6), i.second);
}

////////////////////////////////////////////////////////////////////////////////
// Using the general rc.debug setting automaticalls sets debug.tls, debug.hooks
// and debug.parser, unless they already have values, which by default they do
// not.
void Context::propagateDebug ()
{
  if (config.getBoolean ("debug"))
  {
    if (! config.has ("debug.tls"))
      config.set ("debug.tls", 2);

    if (! config.has ("debug.hooks"))
      config.set ("debug.hooks", 1);

    if (! config.has ("debug.parser"))
      config.set ("debug.parser", 1);
  }
  else
  {
    if ((config.has ("debug.hooks")  && config.getInteger ("debug.hooks")) ||
        (config.has ("debug.parser") && config.getInteger ("debug.parser")) )
      config.set ("debug", true);
  }
}

////////////////////////////////////////////////////////////////////////////////
// No duplicates.
void Context::header (const std::string& input)
{
  if (input.length () &&
      std::find (headers.begin (), headers.end (), input) == headers.end ())
    headers.push_back (input);
}

////////////////////////////////////////////////////////////////////////////////
// No duplicates.
void Context::footnote (const std::string& input)
{
  if (input.length () &&
      std::find (footnotes.begin (), footnotes.end (), input) == footnotes.end ())
    footnotes.push_back (input);
}

////////////////////////////////////////////////////////////////////////////////
// No duplicates.
void Context::error (const std::string& input)
{
  if (input.length () &&
      std::find (errors.begin (), errors.end (), input) == errors.end ())
    errors.push_back (input);
}

////////////////////////////////////////////////////////////////////////////////
void Context::debug (const std::string& input)
{
  if (input.length ())
    debugMessages.push_back (input);
}

////////////////////////////////////////////////////////////////////////////////
void Context::clearMessages ()
{
  headers.clear ();
  footnotes.clear ();
  errors.clear ();
  debugMessages.clear ();
}

////////////////////////////////////////////////////////////////////////////////
