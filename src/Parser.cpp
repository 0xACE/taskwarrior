////////////////////////////////////////////////////////////////////////////////
//
// Copyright 2006 - 2014, Paul Beckingham, Federico Hernandez.
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
#include <stdlib.h>
#include <unistd.h>
#include <Context.h>
#include <Parser.h>
#include <Lexer.h>
#include <Nibbler.h>
#include <Directory.h>
#include <main.h>
#include <text.h>
#include <util.h>
#include <i18n.h>

#ifdef FEATURE_STDIN
#include <sys/select.h>
#endif

extern Context context;

// Overridden by rc.abbreviation.minimum.
static int minimumMatchLength = 3;

// Alias expansion limit. Any more indicates some kind of error.
static int safetyValveDefault = 10;

////////////////////////////////////////////////////////////////////////////////
Parser::Parser ()
{
  _tree = new Tree ("root");
}

////////////////////////////////////////////////////////////////////////////////
Parser::~Parser ()
{
  delete _tree;
}

////////////////////////////////////////////////////////////////////////////////
// char** argv --> std::vector <std::string> _args
void Parser::initialize (int argc, const char** argv)
{
  // Set up constants.
  int override = context.config.getInteger ("abbreviation.minimum");
  if (override)
    minimumMatchLength = override;

  // Create top-level nodes.
  for (int i = 0; i < argc; ++i)
  {
    std::string raw = argv[i];
    Tree* branch = _tree->addBranch (new Tree (format ("arg{1}", i)));
    branch->attribute ("raw", raw);
    branch->tag ("ORIGINAL");
    branch->tag ("?");

    // If the argument contains a space, it was quoted.  Record that fact.
    if (! noSpaces (raw))
      branch->tag ("QUOTED");

    // Lex each argument.  If there are multiple lexemes, create sub branches,
    // otherwise no change.
    std::string lexeme;
    Lexer::Type type;
    Lexer lex (raw);
    lex.ambiguity (false);

    std::vector <std::pair <std::string, Lexer::Type> > lexemes;
    while (lex.token (lexeme, type))
      lexemes.push_back (std::pair <std::string, Lexer::Type> (lexeme, type));

    if (lexemes.size () > 1)
    {
      std::vector <std::pair <std::string, Lexer::Type> >::iterator l;
      for (l = lexemes.begin (); l != lexemes.end (); ++l)
      {
        Tree* sub = branch->addBranch (new Tree ("argSub"));
        sub->attribute ("raw", l->first);

        if (l->second == Lexer::typeOperator)
          sub->tag ("OP");
        else
          sub->tag ("?");

        // TODO More types needed.  Perhaps.
      }
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
void Parser::clear ()
{
  delete _tree;
  _tree = new Tree ("root");
}

////////////////////////////////////////////////////////////////////////////////
// Add an arg for every word from std::cin.
//
// echo one two -- three | task zero --> task zero one two
// 'three' is left in the input buffer.
void Parser::appendStdin ()
{
#ifdef FEATURE_STDIN
  // Use 'select' to determine whether there is any std::cin content buffered
  // before trying to read it, to prevent blocking.
  struct timeval tv;
  tv.tv_sec = 0;
  tv.tv_usec = 1000;

  fd_set fds;
  FD_ZERO (&fds);
  FD_SET (STDIN_FILENO, &fds);

  int result = select (STDIN_FILENO + 1, &fds, NULL, NULL, &tv);
  if (result && result != -1)
  {
    if (FD_ISSET (0, &fds))
    {
      int i = 0;
      std::string arg;
      while (std::cin >> arg)
      {
        // It the terminator token is found, stop reading.
        if (arg == "--")
          break;

        Tree* branch = _tree->addBranch (new Tree (format ("stdin{1}", i++)));
        branch->attribute ("raw", arg);
        branch->tag ("ORIGINAL");
        branch->tag ("STDIN");
        branch->tag ("?");
      }
    }
  }
#endif
}

////////////////////////////////////////////////////////////////////////////////
Tree* Parser::tree ()
{
  return _tree;
}

////////////////////////////////////////////////////////////////////////////////
Tree* Parser::parse ()
{
  findBinary ();
  scan (&Parser::findTerminator);
  resolveAliases ();

  findOverrides ();
  applyOverrides ();

  scan (&Parser::findSubstitution);
  scan (&Parser::findPattern);
  findTag ();
  scan (&Parser::findAttribute);
  scan (&Parser::findAttributeModifier);
  findOperator ();
  findCommand ();
  findUUIDList ();
  findIdSequence ();
  findFilter ();
  findModifications ();
  findStrayModifications ();

  findPlainArgs ();
  findMissingOperators ();

  validate ();
  return _tree;
}

////////////////////////////////////////////////////////////////////////////////
void Parser::alias (const std::string& name, const std::string& value)
{
  _aliases.insert (std::pair <std::string, std::string> (name, value));
}

////////////////////////////////////////////////////////////////////////////////
void Parser::entity (const std::string& name, const std::string& value)
{
  _entities.insert (std::pair <std::string, std::string> (name, value));
}

////////////////////////////////////////////////////////////////////////////////
// Search for exact 'value' in _entities category.
bool Parser::exactMatch (
  const std::string& category,
  const std::string& value) const
{
  // Find the category.
  std::pair <std::multimap <std::string, std::string>::const_iterator, std::multimap <std::string, std::string>::const_iterator> c;
  c = _entities.equal_range (category);

  // Extract a list of entities for category.
  std::vector <std::string> options;
  std::multimap <std::string, std::string>::const_iterator e;
  for (e = c.first; e != c.second; ++e)
  {
    // Shortcut: if an exact match is found, success.
    if (value == e->second)
      return true;
  }

  return false;
}

////////////////////////////////////////////////////////////////////////////////
// Search for 'value' in _entities category, return canonicalized value.
bool Parser::canonicalize (
  std::string& canonicalized,
  const std::string& category,
  const std::string& value) const
{
  // Find the category.
  std::pair <std::multimap <std::string, std::string>::const_iterator, std::multimap <std::string, std::string>::const_iterator> c;
  c = _entities.equal_range (category);

  // Extract a list of entities for category.
  std::vector <std::string> options;
  std::multimap <std::string, std::string>::const_iterator e;
  for (e = c.first; e != c.second; ++e)
  {
    // Shortcut: if an exact match is found, success.
    if (value == e->second)
    {
      canonicalized = value;
      return true;
    }

    options.push_back (e->second);
  }

  // Match against the options, throw away results.
  std::vector <std::string> matches;
  if (autoComplete (value, options, matches, minimumMatchLength) == 1)
  {
    canonicalized = matches[0];
    return true;
  }

  return false;
}

////////////////////////////////////////////////////////////////////////////////
// Recursively scan all nodes, depth first, skipping terminated nodes, and known
// nodes, and cal the callback function for each node.
void Parser::scan (void (Parser::*callback) (Tree*), Tree* tree /* = NULL */)
{
  if (tree == NULL)
    tree = _tree;

  std::vector <Tree*>::iterator i;
  for (i = tree->_branches.begin (); i != tree->_branches.end (); ++i)
  {
    if ((*i)->_branches.size ())
    {
      scan (callback, *i);
    }
    else
    {
      // Parser override operator.
      if ((*i)->hasTag ("TERMINATOR") ||
          (*i)->hasTag ("TERMINATED"))
        break;

      // Skip known args.
      if (! (*i)->hasTag ("?"))
        continue;

      (this->*callback) (*i);
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
// Locate and tag the binary.
void Parser::findBinary ()
{
  if (_tree->_branches.size () >= 1)
  {
    _tree->_branches[0]->unTag ("?");
    _tree->_branches[0]->removeAllBranches ();
    _tree->_branches[0]->tag ("BINARY");
    std::string binary = _tree->_branches[0]->attribute ("raw");
    std::string::size_type slash = binary.rfind ('/');
    if (slash != std::string::npos)
    {
      binary = binary.substr (slash + 1);
    }

    _tree->_branches[0]->attribute ("basename", binary);

    if (binary == "cal" || binary == "calendar")
    {
      _tree->_branches[0]->tag ("CALENDAR");
    }
    else if (binary == "task" || binary == "tw" || binary == "t")
    {
      _tree->_branches[0]->tag ("TW");
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
// The parser override operator terminates all subsequent cleverness, leaving
// all args in the raw state.
void Parser::findTerminator (Tree* t)
{
  // Mark the terminator.
  static bool found = false;
  if (! found &&
      t->attribute ("raw") == "--")
  {
    t->unTag ("?");
    t->removeAllBranches ();
    t->tag ("TERMINATOR");
    found = true;
  }

  // Mark subsequent nodes.
  else if (found)
  {
    t->unTag ("?");
    t->removeAllBranches ();
    t->tag ("WORD");
    t->tag ("TERMINATED");
  }
}

////////////////////////////////////////////////////////////////////////////////
// TODO This is not recusive, and needs to be.
void Parser::resolveAliases ()
{
  bool something;
  int safety_valve = safetyValveDefault;

  do
  {
    something = false;
    std::vector <Tree*>::iterator i;
    for (i = _tree->_branches.begin (); i != _tree->_branches.end (); ++i)
    {
      // Parser override operator.
      if ((*i)->attribute ("raw") == "--")
        break;

      std::map <std::string, std::string>::iterator a;
      if ((*i)->_branches.size ())
      {
        std::vector <Tree*>::iterator b;
        for (b = (*i)->_branches.begin (); b != (*i)->_branches.end (); ++b)
        {
          a = _aliases.find ((*b)->attribute ("raw"));
          if (a != _aliases.end ())
          {
            (*b)->attribute ("raw", a->second);
            (*b)->tag ("ALIAS");
            something = true;
          }
        }
      }
      else
      {
        a = _aliases.find ((*i)->attribute ("raw"));
        if (a != _aliases.end ())
        {
          (*i)->attribute ("raw", a->second);
          (*i)->tag ("ALIAS");
          something = true;
        }
      }
    }
  }
  while (something && --safety_valve > 0);

  if (safety_valve <= 0)
    context.debug (format (STRING_PARSER_ALIAS_NEST, safetyValveDefault));
}

////////////////////////////////////////////////////////////////////////////////
// Walk the top-level tree branches, looking for the first raw value that
// autoCompletes to a valid command/report.
void Parser::findCommand ()
{
  // There can be only one.
  // Scan for an existing CMD tag, to short-circuit scanning for another.
  std::string command;
  std::vector <Tree*>::iterator i;
  for (i = _tree->_branches.begin (); i != _tree->_branches.end (); ++i)
    if ((*i)->hasTag ("CMD"))
      return;

  // No CMD tag found, now look for a command.
  for (i = _tree->_branches.begin (); i != _tree->_branches.end (); ++i)
  {
    // Parser override operator.
    if ((*i)->attribute ("raw") == "--")
      break;

    // Skip known args.
    if (! (*i)->hasTag ("?"))
      continue;

    if (canonicalize (command, "cmd", (*i)->attribute ("raw")))
    {
      (*i)->unTag ("?");
      (*i)->removeAllBranches ();
      (*i)->tag ("CMD");
      (*i)->attribute ("canonical", command);

           if (exactMatch ("writecmd", command)) (*i)->tag ("WRITECMD");
      else if (exactMatch ("readcmd",  command)) (*i)->tag ("READCMD");
      else if (exactMatch ("helper",   command)) (*i)->tag ("HELPER");

      // Stop at the first command found.
      return;
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
// rc:<file>
// rc.<name>[:=]<value>
void Parser::findOverrides ()
{
  std::vector <Tree*>::iterator i;
  for (i = _tree->_branches.begin (); i != _tree->_branches.end (); ++i)
  {
    // Parser override operator.
    if ((*i)->attribute ("raw") == "--")
      break;

    // Skip known args.
    if (! (*i)->hasTag ("?"))
      continue;

    std::string arg = (*i)->attribute ("raw");
    if (arg.find ("rc:") == 0)
    {
      (*i)->unTag ("?");
      (*i)->removeAllBranches ();
      (*i)->tag ("RC");
      (*i)->attribute ("file", arg.substr (3));
    }
    else if (arg.find ("rc.") == 0)
    {
      std::string::size_type sep = arg.find ('=', 3);
      if (sep == std::string::npos)
        sep = arg.find (':', 3);

      if (sep != std::string::npos)
      {
        (*i)->unTag ("?");
        (*i)->removeAllBranches ();
        (*i)->tag ("CONFIG");
        (*i)->attribute ("name", arg.substr (3, sep - 3));
        (*i)->attribute ("value", arg.substr (sep + 1));
      }
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
// Look for RC and return file as a File.
void Parser::getOverrides (
  std::string& home,
  File& rc)
{
  std::vector <Tree*>::iterator i;
  for (i = _tree->_branches.begin (); i != _tree->_branches.end (); ++i)
  {
    if ((*i)->hasTag ("RC"))
    {
      rc = File ((*i)->attribute ("file"));
      home = rc;

      std::string::size_type last_slash = rc._data.rfind ("/");
      if (last_slash != std::string::npos)
        home = rc._data.substr (0, last_slash);
      else
        home = ".";

      context.header (format (STRING_PARSER_ALTERNATE_RC, rc._data));

      // Keep looping, because if there are multiple rc:file arguments, we
      // want the last one to dominate.
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
// Look for CONFIG data.location and return value as a Path.
void Parser::getDataLocation (Path& data)
{
  std::string location = context.config.get ("data.location");
  if (location != "")
    data = location;

  std::vector <Tree*>::iterator i;
  for (i = _tree->_branches.begin (); i != _tree->_branches.end (); ++i)
  {
    if ((*i)->hasTag ("CONFIG") &&
        (*i)->attribute ("name") == "data.location")
    {
      data = Directory ((*i)->attribute ("value"));
      context.header (format (STRING_PARSER_ALTERNATE_DATA, (std::string) data));
    }

    // Keep looping, because if there are multiple overrides, we want the last
    // one to dominate.
  }
}

////////////////////////////////////////////////////////////////////////////////
// Takes all CONFIG name/value pairs and overrides configuration.
// leaving only the plain args.
void Parser::applyOverrides ()
{
  std::vector <Tree*>::iterator i;
  for (i = _tree->_branches.begin (); i != _tree->_branches.end (); ++i)
  {
    if ((*i)->hasTag ("CONFIG"))
    {
      std::string name  = (*i)->attribute ("name");
      std::string value = (*i)->attribute ("value");
      context.config.set (name, value);
      context.footnote (format (STRING_PARSER_OVERRIDE_RC, name, value));
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
void Parser::injectDefaults ()
{
  // Scan the top-level branches for evidence of ID, UUID, overrides and other
  // arguments.
  bool found_command  = false;
  bool found_sequence = false;
  bool found_other    = false;
  std::vector <Tree*>::iterator i;
  for (i = _tree->_branches.begin (); i != _tree->_branches.end (); ++i)
  {
    if ((*i)->hasTag ("BINARY"))
      ;

    else if ((*i)->hasTag ("CMD"))
      found_command = true;

    else if ((*i)->hasTag ("ID") || (*i)->hasTag ("UUID"))
      found_sequence = true;

    else if (! (*i)->hasTag ("RC") &&
             ! (*i)->hasTag ("CONFIG"))
      found_other = true;
  }

  // If no command was specified, then a command will be inserted.
  if (! found_command)
  {
    // Default command.
    if (! found_sequence)
    {
      // Apply overrides, if any.
      std::string defaultCommand = context.config.get ("default.command");
      if (defaultCommand != "")
      {
        context.debug ("No command or sequence found - assuming default.command.");

        // Split the defaultCommand into args, and add them in reverse order,
        // because captureFirst inserts args immediately after the command, and
        // so has the effect of reversing the list.
        std::vector <std::string> args;
        split (args, defaultCommand, ' ');
        std::vector <std::string>::reverse_iterator r;
        for (r = args.rbegin (); r != args.rend (); ++r)
        {
          if (*r != "")
          {
            Tree* t = captureFirst (*r);
            t->tag ("DEFAULT");
          }
        }

        std::string combined;
        std::vector <Tree*>::iterator i;
        for (i = _tree->_branches.begin (); i != _tree->_branches.end (); ++i)
        {
          if (combined.length ())
            combined += ' ';

          combined += (*i)->attribute ("raw");
        }

        context.header ("[" + combined + "]");
      }
      else
      {
        throw std::string (STRING_TRIVIAL_INPUT);
      }
    }
    else
    {
      // TODO There is a problem, in that this block is not run.

      // Information command.
      if (! found_other)
      {
        context.debug ("Sequence but no command found - assuming 'information' command.");
        context.header (STRING_ASSUME_INFO);
        Tree* t = captureFirst ("information");
        t->tag ("ASSUMED");
      }
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
Tree* Parser::captureFirst (const std::string& arg)
{
  // Insert the arg as the new first branch.
  Tree* t = new Tree ("argIns");
  if (!t)
    throw std::string (STRING_ERROR_MEMORY);

  t->attribute ("raw", arg);
  t->tag ("?");
  t->_trunk = _tree;

  std::vector <Tree*>::iterator i = _tree->_branches.begin ();
  if (i != _tree->_branches.end ())
    i++;  // Walk past the binary.

  _tree->_branches.insert (i, t);

  findBinary ();
  findCommand ();
  return t;
}

////////////////////////////////////////////////////////////////////////////////
Tree* Parser::captureLast (const std::string& arg)
{
  // Insert the arg as the new first branch.
  Tree* t = new Tree ("argIns");
  if (!t)
    throw std::string (STRING_ERROR_MEMORY);

  t->attribute ("raw", arg);
  t->tag ("?");
  t->_trunk = _tree;

  _tree->_branches.push_back (t);
  return t;
}

////////////////////////////////////////////////////////////////////////////////
const std::string Parser::getFilterExpression ()
{
  // Construct an efficient ID/UUID clause.
  std::string sequence = "";
  std::vector <Tree*>::iterator i;
  for (i = _tree->_branches.begin (); i != _tree->_branches.end (); ++i)
  {
    if ((*i)->hasTag ("FILTER") && ! (*i)->hasTag ("PSEUDO"))
    {
      if ((*i)->_branches.size ())
      {
        std::vector <Tree*>::iterator b;
        for (b = (*i)->_branches.begin (); b != (*i)->_branches.end (); ++b)
        {
          if (sequence != "")
            sequence += " ";

          if ((*b)->hasTag ("STRING"))
            sequence += "'" + (*b)->attribute ("raw") + "'";
          else
            sequence += (*b)->attribute ("raw");
        }
      }
      else
      {
        if (sequence != "")
          sequence += " ";

        if ((*i)->hasTag ("STRING"))
          sequence += "'" + (*i)->attribute ("raw") + "'";
        else
          sequence += (*i)->attribute ("raw");
      }
    }
  }

  if (sequence != "")
    sequence = "( " + sequence + " )";

  if (context.verbose ("filter"))
    context.footnote ("Filter: " + sequence);

  return sequence;
}

////////////////////////////////////////////////////////////////////////////////
const std::vector <std::string> Parser::getWords () const
{
  std::vector <std::string> words;
  std::vector <Tree*>::const_iterator i;
  for (i = _tree->_branches.begin (); i != _tree->_branches.end (); ++i)
  {
    if (! (*i)->hasTag ("BINARY") &&
        ! (*i)->hasTag ("RC")     &&
        ! (*i)->hasTag ("CONFIG") &&
        ! (*i)->hasTag ("CMD")    &&
        ! (*i)->hasTag ("TERMINATOR") &&
        (*i)->hasTag ("ORIGINAL"))
      words.push_back ((*i)->attribute ("raw"));
  }

  return words;
}

////////////////////////////////////////////////////////////////////////////////
std::string Parser::getLimit () const
{
  std::vector <Tree*>::const_iterator i;
  for (i = _tree->_branches.begin (); i != _tree->_branches.end (); ++i)
  {
    // Parser override operator.
    if ((*i)->attribute ("raw") == "--")
      break;

    if ((*i)->hasTag ("PSEUDO") &&
        (*i)->attribute ("name") == "limit")
    {
      return (*i)->attribute ("raw");
    }
  }

  return "0";
}

////////////////////////////////////////////////////////////////////////////////
std::string Parser::getCommand () const
{
  std::vector <Tree*>::const_iterator i;
  for (i = _tree->_branches.begin (); i != _tree->_branches.end (); ++i)
  {
    // Parser override operator.
    if ((*i)->attribute ("raw") == "--")
      break;

    if ((*i)->hasTag ("CMD"))
      return (*i)->attribute ("canonical");
  }

  return "";
}

////////////////////////////////////////////////////////////////////////////////
// /pattern/ --> description ~ pattern
void Parser::findPattern (Tree* t)
{
  context.debug ("findPattern");
  context.debug (t->dump ());

  Nibbler n (t->attribute ("raw"));
  std::string pattern;
  if (n.getQuoted ('/', pattern) &&
      n.depleted () &&
      pattern.length () > 0)
  {
    t->unTag ("?");
    t->removeAllBranches ();
    t->tag ("PATTERN");

    Tree* branch = t->addBranch (new Tree ("argPat"));
    branch->attribute ("raw", "description");

    branch = t->addBranch (new Tree ("argPat"));
    branch->attribute ("raw", "~");
    branch->tag ("OP");

    branch = t->addBranch (new Tree ("argPat"));
    branch->attribute ("raw", pattern);
    branch->tag ("STRING");
  }
}

////////////////////////////////////////////////////////////////////////////////
// /from/to/[g]
void Parser::findSubstitution (Tree* t)
{
  context.debug ("findSubstitution");
  context.debug (t->dump ());

  std::string raw = t->attribute ("raw");
  Nibbler n (raw);

  std::string from;
  std::string to;
  bool global = false;
  if (n.getQuoted ('/', from) &&
      n.backN ()              &&
      n.getQuoted ('/', to))
  {
    if (n.skip ('g'))
      global = true;

    if (n.depleted () &&
        !Directory (raw).exists ())
    {
      t->unTag ("?");
      t->removeAllBranches ();
      t->tag ("SUBSTITUTION");
      t->attribute ("from", from);
      t->attribute ("to", to);
      t->attribute ("global", global ? 1 : 0);
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
// +tag
void Parser::findTag ()
{
  std::vector <Tree*>::iterator i;
  for (i = _tree->_branches.begin (); i != _tree->_branches.end (); ++i)
  {
    // Parser override operator.
    if ((*i)->attribute ("raw") == "--")
      break;

    // Skip known args.
    if (! (*i)->hasTag ("?"))
      continue;

    std::string raw = (*i)->attribute ("raw");
    Nibbler n (raw);

    std::string tag;
    std::string sign;
    if (n.getN (1, sign)             &&
        (sign == "+" || sign == "-") &&
        n.getUntilEOS (tag)          &&
        tag.find (' ') == std::string::npos)
    {
      (*i)->unTag ("?");
      (*i)->removeAllBranches ();
      (*i)->tag ("TAG");
      (*i)->attribute ("sign", sign);
      (*i)->attribute ("tag", tag);

      Tree* branch = (*i)->addBranch (new Tree ("argTag"));
      branch->attribute ("raw", "tags");

      branch = (*i)->addBranch (new Tree ("argTag"));
      branch->attribute ("raw", (sign == "+" ? "_hastag_" : "_notag_"));
      branch->tag ("OP");

      branch = (*i)->addBranch (new Tree ("argTag"));
      branch->attribute ("raw", tag);
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
// <name>:['"][<value>]['"]
void Parser::findAttribute (Tree* t)
{
  context.debug ("findAttribute");
  context.debug (t->dump ());

  std::string raw = t->attribute ("raw");
  Nibbler n (raw);

  // Look for a valid attribute name.
  std::string name;
  if (n.getName (name) &&
      name.length ())
  {
    if (n.skip (':'))
    {
      std::string value;
      if (n.getQuoted   ('"', value)  ||
          n.getQuoted   ('\'', value) ||
          n.getUntilEOS (value)       ||
          n.depleted ())
      {
        if (value == "")
          value = "''";

        std::string canonical;
        if (canonicalize (canonical, "uda", name))
        {
          t->tag ("UDA");
          t->tag ("MODIFIABLE");
        }

        else if (canonicalize (canonical, "pseudo", name))
        {
          t->unTag ("?");
          t->removeAllBranches ();
          t->tag ("PSEUDO");
          t->attribute ("name", canonical);
          t->attribute ("raw", value);
        }

        else if (canonicalize (canonical, "attribute", name))
        {
          t->unTag ("?");
          t->removeAllBranches ();
          t->tag ("ATTRIBUTE");
          t->attribute ("name", canonical);
          t->attribute ("raw", value);

          std::map <std::string, Column*>::const_iterator col;
          col = context.columns.find (canonical);
          if (col != context.columns.end () &&
              col->second->modifiable ())
          {
            t->tag ("MODIFIABLE");
          }

          Tree* branch = t->addBranch (new Tree ("argAtt"));
          branch->attribute ("raw", canonical);

          branch = t->addBranch (new Tree ("argAtt"));
          branch->tag ("OP");

          // All 'project' attributes are partial matches.
          if (canonical == "project" ||
              canonical == "uuid")
            branch->attribute ("raw", "=");
          else
            branch->attribute ("raw", "==");

          branch = t->addBranch (new Tree ("argAtt"));
          branch->attribute ("raw", value);
        }
      }
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
// <name>.<mod>[:=]['"]<value>['"]
void Parser::findAttributeModifier (Tree* t)
{
  context.debug ("findAttributeModifier");
  context.debug (t->dump ());

  std::string raw = t->attribute ("raw");
  Nibbler n (raw);

  std::string name;
  if (n.getUntil (".", name))
  {
    std::string canonical;
    if (canonicalize (canonical, "attribute", name) ||
        canonicalize (canonical, "uda",       name))
    {
      if (n.skip ('.'))
      {
        std::string sense = "+";
        if (n.skip ('~'))
          sense = "-";

        std::string modifier;
        n.getUntilOneOf (":=", modifier);

        if (n.skip (':') ||
            n.skip ('='))
        {
          std::string value;
          if (n.getQuoted   ('"', value)  ||
              n.getQuoted   ('\'', value) ||
              n.getUntilEOS (value)       ||
              n.depleted ())
          {
            if (value == "")
              value = "''";

            t->unTag ("?");
            t->removeAllBranches ();
            t->tag ("ATTMOD");
            t->attribute ("name", canonical);
            t->attribute ("raw", value);
            t->attribute ("modifier", modifier);
            t->attribute ("sense", sense);

            Tree* branch = t->addBranch (new Tree ("argAttmod"));
            branch->attribute ("raw", canonical);

            if (modifier == "before" || modifier == "under" || modifier == "below")
            {
              branch = t->addBranch (new Tree ("argAttmod"));
              branch->attribute ("raw", "<");
              branch->tag ("OP");

              branch = t->addBranch (new Tree ("argAttmod"));
              branch->attribute ("raw", value);
            }
            else if (modifier == "after" || modifier == "over" || modifier == "above")
            {
              branch = t->addBranch (new Tree ("argAttmod"));
              branch->attribute ("raw", ">");
              branch->tag ("OP");

              branch = t->addBranch (new Tree ("argAttmod"));
              branch->attribute ("raw", value);
            }
            else if (modifier == "none")
            {
              branch = t->addBranch (new Tree ("argAttmod"));
              branch->attribute ("raw", "==");
              branch->tag ("OP");

              branch = t->addBranch (new Tree ("argAttmod"));
              branch->attribute ("raw", "''");
            }
            else if (modifier == "any")
            {
              branch = t->addBranch (new Tree ("argAttmod"));
              branch->attribute ("raw", "!=");
              branch->tag ("OP");

              branch = t->addBranch (new Tree ("argAttmod"));
              branch->attribute ("raw", "''");
            }
            else if (modifier == "is" || modifier == "equals")
            {
              branch = t->addBranch (new Tree ("argAttmod"));
              branch->attribute ("raw", "==");
              branch->tag ("OP");

              branch = t->addBranch (new Tree ("argAttmod"));
              branch->attribute ("raw", value);
            }
            else if (modifier == "isnt" || modifier == "not")
            {
              branch = t->addBranch (new Tree ("argAttmod"));
              branch->attribute ("raw", "!=");
              branch->tag ("OP");

              branch = t->addBranch (new Tree ("argAttmod"));
              branch->attribute ("raw", value);
            }
            else if (modifier == "has" || modifier == "contains")
            {
              branch = t->addBranch (new Tree ("argAttmod"));
              branch->attribute ("raw", "~");
              branch->tag ("OP");

              branch = t->addBranch (new Tree ("argAttmod"));
              branch->attribute ("raw", value);
            }
            else if (modifier == "hasnt")
            {
              branch = t->addBranch (new Tree ("argAttmod"));
              branch->attribute ("raw", "!~");
              branch->tag ("OP");

              branch = t->addBranch (new Tree ("argAttmod"));
              branch->attribute ("raw", value);
            }
            else if (modifier == "startswith" || modifier == "left")
            {
              branch = t->addBranch (new Tree ("argAttmod"));
              branch->attribute ("raw", "~");
              branch->tag ("OP");

              branch = t->addBranch (new Tree ("argAttmod"));
              branch->attribute ("raw", "'^" + value + "'");
            }
            else if (modifier == "endswith" || modifier == "right")
            {
              branch = t->addBranch (new Tree ("argAttmod"));
              branch->attribute ("raw", "~");
              branch->tag ("OP");

              branch = t->addBranch (new Tree ("argAttmod"));
              branch->attribute ("raw", "'" + value + "$'");
            }
            else if (modifier == "word")
            {
              branch = t->addBranch (new Tree ("argAttmod"));
              branch->attribute ("raw", "~");
              branch->tag ("OP");

              branch = t->addBranch (new Tree ("argAttmod"));

#if defined (DARWIN)
              branch->attribute ("raw", value);
#elif defined (SOLARIS)
              branch->attribute ("raw", "'\\<" + value + "\\>'");
#else
              branch->attribute ("raw", "'\\b" + value + "\\b'");
#endif
            }
            else if (modifier == "noword")
            {
              branch = t->addBranch (new Tree ("argAttmod"));
              branch->attribute ("raw", "!~");
              branch->tag ("OP");

              branch = t->addBranch (new Tree ("argAttmod"));

#if defined (DARWIN)
              branch->attribute ("raw", value);
#elif defined (SOLARIS)
              branch->attribute ("raw", "'\\<" + value + "\\>'");
#else
              branch->attribute ("raw", "'\\b" + value + "\\b'");
#endif
            }
            else
              throw format (STRING_PARSER_UNKNOWN_ATTMOD, modifier);

            std::map <std::string, Column*>::const_iterator col;
            col = context.columns.find (canonical);
            if (col != context.columns.end () &&
                col->second->modifiable ())
            {
              t->tag ("MODIFIABLE");
            }
          }
        }
      }
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
// A sequence can be:
//
//   a single ID:          1
//   a list of IDs:        1,3,5
//   a list of IDs:        1 3 5
//   a range:              5-10
//   or a combination:     1,3,5-10 12
//
void Parser::findIdSequence ()
{
  std::vector <Tree*>::iterator i;
  for (i = _tree->_branches.begin (); i != _tree->_branches.end (); ++i)
  {
    // Parser override operator.
    if ((*i)->attribute ("raw") == "--")
      break;

    // Skip known args.
    if (! (*i)->hasTag ("?"))
      continue;

    // Container for min/max ID ranges.
    std::vector <std::pair <int, int> > ranges;

    // Split the ID list into elements.
    std::string raw = (*i)->attribute ("raw");
    std::vector <std::string> elements;
    split (elements, raw, ',');

    bool not_an_id = false;
    std::vector <std::string>::iterator e;
    for (e = elements.begin (); e != elements.end (); ++e)
    {
      // Split the ID range into min/max.
      std::vector <std::string> terms;
      split (terms, *e, '-');

      if (terms.size () == 1)
      {
        if (! digitsOnly (terms[0]))
        {
          not_an_id = true;
          break;
        }

        Nibbler n (terms[0]);
        int id;
        if (n.getUnsignedInt (id) &&
            n.depleted ())
        {
          ranges.push_back (std::pair <int, int> (id, id));
        }
        else
        {
          not_an_id = true;
          break;
        }
      }
      else if (terms.size () == 2)
      {
        if (! digitsOnly (terms[0]) ||
            ! digitsOnly (terms[1]))
        {
          not_an_id = true;
          break;
        }

        Nibbler n_min (terms[0]);
        Nibbler n_max (terms[1]);
        int id_min;
        int id_max;
        if (n_min.getUnsignedInt (id_min) &&
            n_min.depleted ()             &&
            n_max.getUnsignedInt (id_max) &&
            n_max.depleted ())
        {
          if (id_min > id_max)
            throw std::string (STRING_PARSER_RANGE_INVERTED);

          ranges.push_back (std::pair <int, int> (id_min, id_max));
        }
        else
        {
          not_an_id = true;
          break;
        }
      }
    }

    if (not_an_id)
      continue;

    // Now convert the ranges into an infix expression.
    (*i)->unTag ("?");
    (*i)->removeAllBranches ();
    (*i)->tag ("ID");

    Tree* branch = (*i)->addBranch (new Tree ("argSeq"));
    branch->attribute ("raw", "(");
    branch->tag ("OP");

    std::vector <std::pair <int, int> >::iterator r;
    for (r = ranges.begin (); r != ranges.end (); ++r)
    {
      if (r != ranges.begin ())
      {
        branch = (*i)->addBranch (new Tree ("argSeq"));
        branch->attribute ("raw", "or");
        branch->tag ("OP");
      }

      if (r->first == r->second)
      {
        branch = (*i)->addBranch (new Tree ("argSeq"));
        branch->attribute ("raw", "id");

        branch = (*i)->addBranch (new Tree ("argSeq"));
        branch->attribute ("raw", "==");
        branch->tag ("OP");

        branch = (*i)->addBranch (new Tree ("argSeq"));
        branch->attribute ("raw", r->first);
      }
      else
      {
        branch = (*i)->addBranch (new Tree ("argSeq"));
        branch->attribute ("raw", "(");
        branch->tag ("OP");

        branch = (*i)->addBranch (new Tree ("argSeq"));
        branch->attribute ("raw", "id");

        branch = (*i)->addBranch (new Tree ("argSeq"));
        branch->attribute ("raw", ">=");
        branch->tag ("OP");

        branch = (*i)->addBranch (new Tree ("argSeq"));
        branch->attribute ("raw", r->first);

        branch = (*i)->addBranch (new Tree ("argSeq"));
        branch->attribute ("raw", "and");
        branch->tag ("OP");

        branch = (*i)->addBranch (new Tree ("argSeq"));
        branch->attribute ("raw", "id");

        branch = (*i)->addBranch (new Tree ("argSeq"));
        branch->attribute ("raw", "<=");
        branch->tag ("OP");

        branch = (*i)->addBranch (new Tree ("argSeq"));
        branch->attribute ("raw", r->second);

        branch = (*i)->addBranch (new Tree ("argSeq"));
        branch->attribute ("raw", ")");
        branch->tag ("OP");
      }
    }

    branch = (*i)->addBranch (new Tree ("argSeq"));
    branch->attribute ("raw", ")");
    branch->tag ("OP");
  }
}

////////////////////////////////////////////////////////////////////////////////
void Parser::findUUIDList ()
{
  std::vector <Tree*>::iterator i;
  for (i = _tree->_branches.begin (); i != _tree->_branches.end (); ++i)
  {
    // Parser override operator.
    if ((*i)->attribute ("raw") == "--")
      break;

    // Skip known args.
    if (! (*i)->hasTag ("?"))
      continue;

    std::string raw = (*i)->attribute ("raw");
    Nibbler n (raw);

    std::vector <std::string> sequence;
    std::string uuid;
    if (n.getUUID (uuid) ||
        n.getPartialUUID (uuid))
    {
      sequence.push_back (uuid);

      while (n.skip (','))
      {
        if (!n.getUUID (uuid) &&
            !n.getPartialUUID (uuid))
          throw std::string (STRING_PARSER_UUID_AFTER_COMMA);

        sequence.push_back (uuid);
      }

      if (n.depleted ())
      {
        (*i)->unTag ("?");
        (*i)->removeAllBranches ();
        (*i)->tag ("UUID");

        Tree* branch = (*i)->addBranch (new Tree ("argSeq"));
        branch->attribute ("raw", "(");
        branch->tag ("OP");

        std::vector <std::string>::iterator u;
        for (u = sequence.begin (); u != sequence.end (); ++u)
        {
          if (u != sequence.begin ())
          {
            branch = (*i)->addBranch (new Tree ("argSeq"));
            branch->attribute ("raw", "or");
            branch->tag ("OP");
          }

          branch = (*i)->addBranch (new Tree ("argSeq"));
          branch->attribute ("raw", "uuid");

          branch = (*i)->addBranch (new Tree ("argSeq"));
          branch->attribute ("raw", "=");
          branch->tag ("OP");

          branch = (*i)->addBranch (new Tree ("argSeq"));
          branch->attribute ("raw", "'" + *u + "'");
        }

        branch = (*i)->addBranch (new Tree ("argSeq"));
        branch->attribute ("raw", ")");
        branch->tag ("OP");
      }
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
void Parser::findOperator ()
{
  // Find the category.
  std::pair <std::multimap <std::string, std::string>::const_iterator, std::multimap <std::string, std::string>::const_iterator> c;
  c = _entities.equal_range ("operator");

  // Extract a list of entities for category.
  std::vector <std::string> options;
  std::multimap <std::string, std::string>::const_iterator e;
  for (e = c.first; e != c.second; ++e)
    options.push_back (e->second);

  std::vector <Tree*>::iterator i;
  for (i = _tree->_branches.begin (); i != _tree->_branches.end (); ++i)
  {
    // Parser override operator.
    if ((*i)->attribute ("raw") == "--")
      break;

    // Skip known args.
    if (! (*i)->hasTag ("?"))
      continue;

    std::string raw = (*i)->attribute ("raw");

    std::vector <std::string>::iterator opt;
    for (opt = options.begin (); opt != options.end (); ++opt)
    {
      if (*opt == raw)
      {
        (*i)->unTag ("?");
        (*i)->removeAllBranches ();
        (*i)->tag ("OP");
        break;
      }
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
// Anything before CMD, but not BINARY, RC or CONFIG --> FILTER
// Anything after READCMD, but not BINARY, RC or CONFIG --> FILTER
void Parser::findFilter ()
{
  bool before_cmd = true;
  bool after_readcmd = false;
  std::vector <Tree*>::iterator i;
  for (i = _tree->_branches.begin (); i != _tree->_branches.end (); ++i)
  {
    // Parser override operator.
    if ((*i)->attribute ("raw") == "--")
      break;

    if ((*i)->hasTag ("CMD"))
      before_cmd = false;

    if ((*i)->hasTag ("READCMD"))
      after_readcmd = true;

    if (before_cmd &&
        ! (*i)->hasTag ("CMD") &&
        ! (*i)->hasTag ("BINARY") &&
        ! (*i)->hasTag ("RC") &&
        ! (*i)->hasTag ("CONFIG"))
    {
      (*i)->unTag ("?");
      (*i)->tag ("FILTER");
    }

    if (after_readcmd &&
        ! (*i)->hasTag ("CMD") &&
        ! (*i)->hasTag ("BINARY") &&
        ! (*i)->hasTag ("RC") &&
        ! (*i)->hasTag ("CONFIG"))
    {
      (*i)->unTag ("?");
      (*i)->tag ("FILTER");
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
void Parser::findModifications ()
{
  bool after_writecmd = false;
  std::vector <Tree*>::iterator i;
  for (i = _tree->_branches.begin (); i != _tree->_branches.end (); ++i)
  {
    if ((*i)->hasTag ("WRITECMD"))
      after_writecmd = true;

    if (after_writecmd &&
        ! (*i)->hasTag ("CMD") &&
        ! (*i)->hasTag ("TERMINATOR") &&
        ! (*i)->hasTag ("BINARY") &&
        ! (*i)->hasTag ("RC") &&
        ! (*i)->hasTag ("CONFIG"))
    {
      (*i)->unTag ("?");
      (*i)->tag ("MODIFICATION");
      (*i)->removeAllBranches ();
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
void Parser::findStrayModifications ()
{
  std::string command = getCommand ();
  if (command == "add" ||
      command == "log")
  {
    std::vector <Tree*>::iterator i;
    for (i = _tree->_branches.begin (); i != _tree->_branches.end (); ++i)
    {
      if ((*i)->hasTag ("FILTER"))
      {
        (*i)->unTag ("FILTER");
        (*i)->tag ("MODIFICATION");
        (*i)->removeAllBranches ();
      }
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
// This is called after parsing. The intention is to find plain arguments that
// are not otherwise recognized, and potentially promote them to patterns.
void Parser::findPlainArgs ()
{
  std::vector <Tree*>::iterator i;
  for (i = _tree->_branches.begin (); i != _tree->_branches.end (); ++i)
  {
    if ((*i)->hasTag ("FILTER")   &&
        (*i)->hasTag ("ORIGINAL") &&  // TODO Wrong, can come in through alias/filter
        (*i)->countTags () <= 2)
    {
      // A word can be upgraded to a pattern only if it does not itself contain
      // multiple tokens.
      std::string raw = (*i)->attribute ("raw");
      std::vector <std::string> lexed;
      Lexer::token_split (lexed, raw);
      if (lexed.size () == 1)
      {
        // Furthermore, the word must not canonicalize to an attribute name.
        std::string canonical;
        if (! canonicalize (canonical, "attribute", raw))
        {
          // This tag also prevents further expanѕion.
          (*i)->tag ("PATTERN");

          Tree* branch = (*i)->addBranch (new Tree ("argPat"));
          branch->attribute ("raw", "description");

          branch = (*i)->addBranch (new Tree ("argPat"));
          branch->attribute ("raw", "~");
          branch->tag ("OP");

          branch = (*i)->addBranch (new Tree ("argPat"));
          branch->attribute ("raw", raw);
          branch->tag ("STRING");
        }
      }
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
void Parser::findMissingOperators ()
{
  while (insertOr ())
    ;

  while (insertAnd ())
    ;
}

////////////////////////////////////////////////////////////////////////////////
// Two consecutive ID/UUID arguments get an 'or' inserted between them.
bool Parser::insertOr ()
{
  std::vector <Tree*>::iterator prev = _tree->_branches.begin ();
  std::vector <Tree*>::iterator i;
  for (i = _tree->_branches.begin (); i != _tree->_branches.end (); ++i)
  {
    // Parser override operator.
    if ((*i)->attribute ("raw") == "--")
      break;

    // Skip known args.
    if ((*i)->hasTag ("?"))
      continue;

    if ((*i)->hasTag ("FILTER") && ! (*i)->hasTag ("PSEUDO"))
    {
      if (prev != _tree->_branches.begin () &&
          ((*prev)->hasTag ("ID") || (*prev)->hasTag ("UUID")) &&
          ((*i)->hasTag ("ID") || (*i)->hasTag ("UUID")))
      {
        Tree* branch = new Tree ("argOp");
        branch->attribute ("raw", "or");
        branch->tag ("OP");
        branch->tag ("FILTER");

        _tree->_branches.insert (i, branch);
        return true;
      }
    }

    prev = i;
  }

  return false;
}

////////////////////////////////////////////////////////////////////////////////
// Two consecutive FILTER, non-OP arguments that are not "(" or ")" need an
// "and" operator inserted between them.
//
//   ) <non-op>         -->  ) and <non-op>
//   <non-op> (         -->  <non-op> <and> (
//   ) (                -->  ) and (
//   <non-op> <non-op>  -->  <non-op> and <non-op>
//
bool Parser::insertAnd ()
{
  std::vector <Tree*>::iterator prev = _tree->_branches.begin ();
  std::vector <Tree*>::iterator i;
  for (i = _tree->_branches.begin (); i != _tree->_branches.end (); ++i)
  {
    if ((*i)->hasTag ("FILTER") && ! (*i)->hasTag ("PSEUDO"))
    {
      if ((*i)->_branches.size ())
      {
        std::vector <Tree*>::iterator sub;
        for (sub = (*i)->_branches.begin (); sub != (*i)->_branches.end (); ++sub)
        {
          if (sub != prev &&
              prev != _tree->_branches.begin () &&
              (! (*prev)->hasTag ("OP") || (*prev)->attribute ("raw") == ")") &&
              (! (*sub)->hasTag ("OP") || (*sub)->attribute ("raw") == "("))
          {
            Tree* branch = new Tree ("argOp");
            branch->attribute ("raw", "and");
            branch->tag ("OP");
            branch->tag ("FILTER");
            (*i)->_branches.insert (sub, branch);
            return true;
          }

          prev = sub;
        }
      }
      else
      {
        if (i != prev &&
            prev != _tree->_branches.begin () &&
            (! (*prev)->hasTag ("OP") || (*prev)->attribute ("raw") == ")") &&
            (! (*i)->hasTag ("OP") || (*i)->attribute ("raw") == "("))
        {
          Tree* branch = new Tree ("argOp");
          branch->attribute ("raw", "and");
          branch->tag ("OP");
          branch->tag ("FILTER");
          _tree->_branches.insert (i, branch);
          return true;
        }

        prev = i;
      }
    }
  }

  return false;
}

////////////////////////////////////////////////////////////////////////////////
// Validate the parse tree.
void Parser::validate ()
{
  // Look for any unrecognized original args.
  std::vector <Tree*>::iterator i;
  for (i = _tree->_branches.begin (); i != _tree->_branches.end (); ++i)
    if ((*i)->hasTag ("?"))
      context.debug ("Unrecognized argument '" + (*i)->attribute ("raw") + "'");

  // TODO Any RC node must have a root/+RC @file that exists.
  // TODO There must be a root/+CMD.
}

////////////////////////////////////////////////////////////////////////////////
