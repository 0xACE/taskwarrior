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

#include <Context.h>
#include <Eval.h>
#include <Dates.h>
#include <main.h>
#include <i18n.h>
#include <CmdCalc.h>

extern Context context;

////////////////////////////////////////////////////////////////////////////////
static bool domSource (const std::string& name, Variant& value)
{
  Task t;
  std::string resolved = context.dom.get (name, t);
  if (resolved != name)
  {
    value = Variant (resolved);
    return true;
  }

  return false;
}

////////////////////////////////////////////////////////////////////////////////
CmdCalc::CmdCalc ()
{
  _keyword     = "calc";
  _usage       = "task          calc <expression>";
  _description = STRING_CMD_CALC_USAGE;
  _read_only   = true;
  _displays_id = false;
}

////////////////////////////////////////////////////////////////////////////////
int CmdCalc::execute (std::string& output)
{
  // Configurable infix/postfix
  bool infix = true;
  if (context.config.get ("expressions") == "infix")
    infix = true;
  else if (context.config.get ("expressions") == "postfix")
    infix = false;

  // Create an evaluator with DOM access.
  Eval e;
  e.addSource (namedDates);
  e.addSource (domSource);
  e.ambiguity (false);  // TODO Could/should be configurable.

  // Compile all the args into one expression.
  std::string expression;
  std::vector <std::string> words = context.a3.extract_words ();
  std::vector <std::string>::iterator word;
  for (word = words.begin (); word != words.end (); ++word)
    expression += *word + " ";

  // Evaluate according to preference.
  Variant result;
  if (infix)
    e.evaluateInfixExpression (expression, result);
  else
    e.evaluatePostfixExpression (expression, result);

  output = (std::string) result + "\n";
  return 0;
}

////////////////////////////////////////////////////////////////////////////////
